#include "Engine/Renderer/PointShadowMap.h"
#include "Engine/Core/Logger.h"

using namespace DirectX;

namespace SE {

namespace {
    // D3D11 TextureCube face look/up directions (left-handed coordinate system).
    // Faces: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    constexpr XMFLOAT3 k_lookDirs[6] = {
        {+1,  0,  0}, {-1,  0,  0},
        { 0, +1,  0}, { 0, -1,  0},
        { 0,  0, +1}, { 0,  0, -1}
    };
    constexpr XMFLOAT3 k_upDirs[6] = {
        {0, 1,  0}, {0, 1,  0},
        {0, 0, -1}, {0, 0, +1},
        {0, 1,  0}, {0, 1,  0}
    };
}

XMMATRIX PointShadowMap::FaceView(XMFLOAT3 lightPos, int face)
{
    XMVECTOR eye  = XMLoadFloat3(&lightPos);
    XMVECTOR look = XMLoadFloat3(&k_lookDirs[face]);
    XMVECTOR up   = XMLoadFloat3(&k_upDirs[face]);
    return XMMatrixLookToLH(eye, look, up);
}

bool PointShadowMap::Init(ID3D11Device* device, ShaderLibrary& shaders, uint32_t resolution)
{
    m_resolution = resolution;

    m_perm = shaders.Get(L"Shaders/PointShadowDepth.hlsl");
    if (!m_perm) { SE_LOG_ERROR("PointShadowMap: PointShadowDepth.hlsl failed"); return false; }

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    SE_HR(device->CreateInputLayout(layoutDesc, 5,
        m_perm->vsBlob->GetBufferPointer(), m_perm->vsBlob->GetBufferSize(),
        m_layout.GetAddressOf()));

    // R32_FLOAT cube texture — 6-slice 2D array with TextureCube misc flag
    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = resolution;
    td.Height           = resolution;
    td.MipLevels        = 1;
    td.ArraySize        = 6;
    td.Format           = DXGI_FORMAT_R32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags        = D3D11_RESOURCE_MISC_TEXTURECUBE;
    SE_HR(device->CreateTexture2D(&td, nullptr, m_cubeTex.GetAddressOf()));

    // One RTV per cube face
    for (int f = 0; f < 6; ++f)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
        rtvd.Format                        = DXGI_FORMAT_R32_FLOAT;
        rtvd.ViewDimension                 = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvd.Texture2DArray.MipSlice        = 0;
        rtvd.Texture2DArray.FirstArraySlice = static_cast<UINT>(f);
        rtvd.Texture2DArray.ArraySize       = 1;
        SE_HR(device->CreateRenderTargetView(m_cubeTex.Get(), &rtvd, m_rtv[f].GetAddressOf()));
    }

    // TextureCube SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format                      = DXGI_FORMAT_R32_FLOAT;
    srvd.ViewDimension               = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvd.TextureCube.MipLevels       = 1;
    srvd.TextureCube.MostDetailedMip = 0;
    SE_HR(device->CreateShaderResourceView(m_cubeTex.Get(), &srvd, m_srv.GetAddressOf()));

    // Shared depth buffer — cleared before each face
    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width            = resolution;
    dd.Height           = resolution;
    dd.MipLevels        = 1;
    dd.ArraySize        = 1;
    dd.Format           = DXGI_FORMAT_D32_FLOAT;
    dd.SampleDesc.Count = 1;
    dd.Usage            = D3D11_USAGE_DEFAULT;
    dd.BindFlags        = D3D11_BIND_DEPTH_STENCIL;
    SE_HR(device->CreateTexture2D(&dd, nullptr, m_depthTex.GetAddressOf()));

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};
    dsvd.Format        = DXGI_FORMAT_D32_FLOAT;
    dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    SE_HR(device->CreateDepthStencilView(m_depthTex.Get(), &dsvd, m_dsv.GetAddressOf()));

    // Shadow rasteriser — back-face cull, no depth bias (colour target, bias in shader)
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode        = D3D11_FILL_SOLID;
    rsDesc.CullMode        = D3D11_CULL_BACK;
    rsDesc.DepthClipEnable = TRUE;
    SE_HR(device->CreateRasterizerState(&rsDesc, m_shadowRS.GetAddressOf()));

    if (!m_cb.Create(device)) return false;

    SE_LOG_INFO("PointShadowMap initialised — %ux%u per face", resolution, resolution);
    return true;
}

void PointShadowMap::Shutdown()
{
    m_perm = nullptr;
    m_shadowRS.Reset();
    m_layout.Reset();
    m_dsv.Reset();
    m_depthTex.Reset();
    m_srv.Reset();
    for (int f = 0; f < 6; ++f) m_rtv[f].Reset();
    m_cubeTex.Reset();
}

void PointShadowMap::BeginFace(ID3D11DeviceContext* ctx, int face,
                                XMFLOAT3 lightPos, float lightFar)
{
    // Unbind slots 6-7 to prevent SRV/RTV hazard before binding as RTV
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(6, 1, &nullSRV);
    ctx->PSSetShaderResources(7, 1, &nullSRV);

    // Save current pipeline state
    UINT numVPs = 1;
    ctx->RSGetViewports(&numVPs, &m_savedVP);
    ctx->OMGetRenderTargets(1, &m_savedRTV, &m_savedDSV);
    ctx->RSGetState(&m_savedRS);

    // Set cube-face viewport
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(m_resolution);
    vp.Height   = static_cast<float>(m_resolution);
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    // Bind face RTV + shared DSV; clear both
    ID3D11RenderTargetView* rtv = m_rtv[face].Get();
    const float clearWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // max distance = unshadowed
    ctx->OMSetRenderTargets(1, &rtv, m_dsv.Get());
    ctx->ClearRenderTargetView(rtv, clearWhite);
    ctx->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    ctx->RSSetState(m_shadowRS.Get());

    // Bind depth-pass shaders + input layout
    ctx->VSSetShader(m_perm->vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_perm->ps.Get(), nullptr, 0);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Cache per-face data used by DrawMesh
    m_lightPos     = lightPos;
    m_lightFar     = lightFar;
    XMMATRIX view  = FaceView(lightPos, face);
    XMMATRIX proj  = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.05f, lightFar);
    m_faceViewProj = view * proj;
}

void PointShadowMap::DrawMesh(ID3D11DeviceContext* ctx, const Mesh& mesh, XMMATRIX model)
{
    CBData cb;
    XMStoreFloat4x4(&cb.worldViewProj, model * m_faceViewProj);
    XMStoreFloat4x4(&cb.world,         model);
    cb.lightPos = m_lightPos;
    cb.lightFar = m_lightFar;
    m_cb.Update(ctx, cb);
    m_cb.BindVS(ctx, 0);
    m_cb.BindPS(ctx, 0);

    for (uint32_t i = 0; i < mesh.GetSubMeshCount(); ++i)
        mesh.DrawSubMesh(ctx, i);
}

void PointShadowMap::EndFace(ID3D11DeviceContext* ctx)
{
    ctx->OMSetRenderTargets(1, &m_savedRTV, m_savedDSV);
    ctx->RSSetViewports(1, &m_savedVP);
    ctx->RSSetState(m_savedRS);
    if (m_savedRTV) { m_savedRTV->Release(); m_savedRTV = nullptr; }
    if (m_savedDSV) { m_savedDSV->Release(); m_savedDSV = nullptr; }
    if (m_savedRS)  { m_savedRS->Release();  m_savedRS  = nullptr; }
}

} // namespace SE
