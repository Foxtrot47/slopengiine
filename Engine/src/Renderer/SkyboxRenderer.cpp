#include "Engine/Renderer/SkyboxRenderer.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Core/Logger.h"
#include <d3dcompiler.h>
#include <windows.h>
#include <DirectXTex.h>

namespace SE {

bool SkyboxRenderer::Init(ID3D11Device* device, RenderStateCache& cache, ShaderLibrary& shaders)
{
    const ShaderPermutation* perm = shaders.Get(L"Shaders/Skybox.hlsl");
    if (!perm)
    {
        SE_LOG_ERROR("SkyboxRenderer: failed to compile Skybox.hlsl via ShaderLibrary");
        return false;
    }
    m_vs = perm->vs;
    m_ps = perm->ps;

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    SE_HR(device->CreateInputLayout(layoutDesc, 1,
        perm->vsBlob->GetBufferPointer(), perm->vsBlob->GetBufferSize(), &m_layout));

    if (!m_cb.Create(device)) return false;

    // Unit cube vertices — position only; camera is always inside this cube
    static const DirectX::XMFLOAT3 cubeVerts[8] = {
        {-1,-1,-1}, {+1,-1,-1}, {+1,+1,-1}, {-1,+1,-1},
        {-1,-1,+1}, {+1,-1,+1}, {+1,+1,+1}, {-1,+1,+1},
    };
    static const uint32_t cubeInds[36] = {
        0,1,2, 2,3,0,   // front  (z=-1)
        5,4,7, 7,6,5,   // back   (z=+1)
        4,0,3, 3,7,4,   // left   (x=-1)
        1,5,6, 6,2,1,   // right  (x=+1)
        4,5,1, 1,0,4,   // bottom (y=-1)
        3,2,6, 6,7,3,   // top    (y=+1)
    };
    if (!m_cubeVB.Create(device, cubeVerts, sizeof(cubeVerts), sizeof(DirectX::XMFLOAT3))) return false;
    if (!m_cubeIB.Create(device, cubeInds, 36)) return false;

    // Depth: LESS_EQUAL (sky at NDC z=1 passes cleared buffer), no write
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable    = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    m_depthState = cache.GetDepthStencilState(dsDesc);

    // CULL_NONE — camera is inside the cube so standard winding is backwards
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode        = D3D11_FILL_SOLID;
    rsDesc.CullMode        = D3D11_CULL_NONE;
    rsDesc.DepthClipEnable = TRUE;
    m_rsState = cache.GetRasterizerState(rsDesc);

    // Bilinear: wrap U (longitude), clamp V (avoid seam at poles)
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter        = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU      = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV      = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW      = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy = 1;
    sd.MaxLOD        = D3D11_FLOAT32_MAX;
    SE_HR(device->CreateSamplerState(&sd, &m_sampler));

    return true;
}

bool SkyboxRenderer::LoadPanorama(ID3D11Device* device, const wchar_t* path)
{
    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromDDSFile(path, DirectX::DDS_FLAGS_NONE, nullptr, image);
    if (FAILED(hr))
    {
        char pathA[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, pathA, MAX_PATH, nullptr, nullptr);
        SE_LOG_ERROR("SkyboxRenderer: LoadFromDDSFile failed '%s': 0x%08X", pathA, hr);
        return false;
    }

    const DirectX::TexMetadata& meta = image.GetMetadata();

    // Decompress BC6H → R16G16B16A16_FLOAT so we can generate mips for IBL sampling.
    DirectX::ScratchImage decompressed;
    if (DirectX::IsCompressed(meta.format))
    {
        hr = DirectX::Decompress(image.GetImages(), image.GetImageCount(), meta,
                                 DXGI_FORMAT_R16G16B16A16_FLOAT, decompressed);
        if (FAILED(hr))
        {
            SE_LOG_ERROR("SkyboxRenderer: Decompress failed: 0x%08X", hr);
            return false;
        }
    }
    else
    {
        decompressed = std::move(image);
    }

    // Generate full mip chain for IBL diffuse/specular sampling.
    DirectX::ScratchImage mipped;
    hr = DirectX::GenerateMipMaps(decompressed.GetImages(), decompressed.GetImageCount(),
                                  decompressed.GetMetadata(),
                                  DirectX::TEX_FILTER_LINEAR, 0, mipped);
    if (FAILED(hr))
    {
        SE_LOG_ERROR("SkyboxRenderer: GenerateMipMaps failed: 0x%08X", hr);
        // Fall back to no-mip version
        mipped = std::move(decompressed);
    }

    const DirectX::TexMetadata& mippedMeta = mipped.GetMetadata();
    hr = DirectX::CreateShaderResourceViewEx(
        device,
        mipped.GetImages(), mipped.GetImageCount(), mippedMeta,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        DirectX::CREATETEX_DEFAULT,
        m_panoramaSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        SE_LOG_ERROR("SkyboxRenderer: CreateShaderResourceView failed: 0x%08X", hr);
        return false;
    }

    SE_LOG_INFO("SkyboxRenderer: panorama loaded %zux%zu (%zu mips)",
                mippedMeta.width, mippedMeta.height, mippedMeta.mipLevels);
    return true;
}

void SkyboxRenderer::Draw(ID3D11DeviceContext* ctx, DirectX::XMMATRIX view, DirectX::XMMATRIX proj)
{
    if (!m_panoramaSRV) return;

    // Remove camera translation from view — sky rotates with camera but doesn't translate
    DirectX::XMMATRIX viewRot = view;
    viewRot.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    SkyboxCB cb;
    DirectX::XMStoreFloat4x4(&cb.viewProjNoTrans,
        DirectX::XMMatrixTranspose(viewRot * proj));
    m_cb.Update(ctx, cb);
    m_cb.BindVS(ctx, 0);

    ctx->OMSetDepthStencilState(m_depthState, 0);
    ctx->RSSetState(m_rsState);
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_cubeVB.Bind(ctx, 0);
    m_cubeIB.Bind(ctx);

    ID3D11ShaderResourceView* srv = m_panoramaSRV.Get();
    ctx->PSSetShaderResources(0, 1, &srv);
    ctx->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

    ctx->DrawIndexed(36, 0, 0);

    // Restore defaults so subsequent scene draws are unaffected
    ctx->OMSetDepthStencilState(nullptr, 0);
    ctx->RSSetState(nullptr);
    ID3D11ShaderResourceView* nullSrv = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSrv);
}

} // namespace SE
