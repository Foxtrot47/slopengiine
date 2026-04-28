#include "Engine/Renderer/ShadowMap.h"
#include "Engine/Core/Logger.h"

namespace SE {

bool ShadowMap::Init(ID3D11Device* device, ShaderLibrary& shaders, uint32_t resolution)
{
    m_resolution = resolution;

    m_perm = shaders.Get(L"Shaders/ShadowDepth.hlsl");
    if (!m_perm)
    {
        SE_LOG_ERROR("ShadowMap: failed to compile ShadowDepth.hlsl");
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    SE_HR(device->CreateInputLayout(layoutDesc, 5,
        m_perm->vsBlob->GetBufferPointer(), m_perm->vsBlob->GetBufferSize(), &m_layout));

    // Depth texture
    D3D11_TEXTURE2D_DESC td = {};
    td.Width      = resolution;
    td.Height     = resolution;
    td.MipLevels  = 1;
    td.ArraySize  = 1;
    td.Format     = DXGI_FORMAT_R32_TYPELESS;
    td.SampleDesc = { 1, 0 };
    td.Usage      = D3D11_USAGE_DEFAULT;
    td.BindFlags  = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    SE_HR(device->CreateTexture2D(&td, nullptr, &m_depthTex));

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format        = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    SE_HR(device->CreateDepthStencilView(m_depthTex.Get(), &dsvDesc, &m_dsv));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format              = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    SE_HR(device->CreateShaderResourceView(m_depthTex.Get(), &srvDesc, &m_srv));

    // PCF comparison sampler
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sd.BorderColor[0] = sd.BorderColor[1] = sd.BorderColor[2] = sd.BorderColor[3] = 1.0f;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    SE_HR(device->CreateSamplerState(&sd, &m_shadowSampler));

    if (!m_cb.Create(device)) return false;

    SE_LOG_INFO("ShadowMap initialised — %ux%u", resolution, resolution);
    return true;
}

void ShadowMap::UpdateLightMatrix(DirectX::XMFLOAT3 lightDir,
                                   DirectX::XMFLOAT3 sceneCentre, float sceneRadius)
{
    using namespace DirectX;
    XMVECTOR dir    = XMVector3Normalize(XMLoadFloat3(&lightDir));
    XMVECTOR center = XMLoadFloat3(&sceneCentre);
    XMVECTOR eye    = XMVectorSubtract(center, XMVectorScale(dir, sceneRadius));
    XMVECTOR up     = XMVectorSet(0, 1, 0, 0);
    // If light is nearly vertical, pick a different up vector.
    if (fabsf(XMVectorGetY(dir)) > 0.99f)
        up = XMVectorSet(0, 0, 1, 0);

    XMMATRIX view = XMMatrixLookAtLH(eye, center, up);
    XMMATRIX proj = XMMatrixOrthographicLH(
        sceneRadius * 2.0f, sceneRadius * 2.0f, 0.1f, sceneRadius * 3.0f);
    m_lightViewProj = view * proj;
}

void ShadowMap::BeginShadowPass(ID3D11DeviceContext* ctx)
{
    // Save current state.
    UINT numVPs = 1;
    ctx->RSGetViewports(&numVPs, &m_savedVP);
    ctx->OMGetRenderTargets(1, &m_savedRTV, &m_savedDSV);

    // Set shadow viewport.
    D3D11_VIEWPORT vp = {};
    vp.Width    = (float)m_resolution;
    vp.Height   = (float)m_resolution;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    // Bind depth-only target.
    ID3D11RenderTargetView* nullRTV = nullptr;
    ctx->OMSetRenderTargets(1, &nullRTV, m_dsv.Get());
    ctx->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Bind shadow shaders.
    ctx->VSSetShader(m_perm->vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_perm->ps.Get(), nullptr, 0);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ShadowMap::DrawMesh(ID3D11DeviceContext* ctx, const Mesh& mesh, DirectX::XMMATRIX model)
{
    using namespace DirectX;
    ShadowCBData cb;
    XMStoreFloat4x4(&cb.model, model);
    XMStoreFloat4x4(&cb.viewProj, m_lightViewProj);
    m_cb.Update(ctx, cb);
    m_cb.BindVS(ctx, 0);

    for (uint32_t i = 0; i < mesh.GetSubMeshCount(); ++i)
        mesh.DrawSubMesh(ctx, i);
}

void ShadowMap::EndShadowPass(ID3D11DeviceContext* ctx)
{
    // Restore previous render target and viewport.
    ctx->OMSetRenderTargets(1, &m_savedRTV, m_savedDSV);
    ctx->RSSetViewports(1, &m_savedVP);
    if (m_savedRTV) { m_savedRTV->Release(); m_savedRTV = nullptr; }
    if (m_savedDSV) { m_savedDSV->Release(); m_savedDSV = nullptr; }
}

void ShadowMap::BindForLitPass(ID3D11DeviceContext* ctx)
{
    ID3D11ShaderResourceView* srv = m_srv.Get();
    ctx->PSSetShaderResources(3, 1, &srv);
    ctx->PSSetSamplers(1, 1, m_shadowSampler.GetAddressOf());
}

void ShadowMap::Unbind(ID3D11DeviceContext* ctx)
{
    ID3D11ShaderResourceView* nullSrv = nullptr;
    ctx->PSSetShaderResources(3, 1, &nullSrv);
}

} // namespace SE
