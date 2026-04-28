#include "Engine/Renderer/DeferredPipeline.h"
#include "Engine/Renderer/LightEnvironment.h"
#include "Engine/Renderer/ShadowMap.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Core/Logger.h"

using namespace DirectX;

namespace SE {

bool DeferredPipeline::Init(ID3D11Device* device, AssetManager& /*assets*/,
                             ShaderLibrary& shaders)
{
    // Geometry pass shader
    m_geomPerm = shaders.Get(L"Shaders/GBufferFill.hlsl");
    if (!m_geomPerm) { SE_LOG_ERROR("DeferredPipeline: GBufferFill.hlsl failed"); return false; }

    // Lighting pass shader
    m_lightPerm = shaders.Get(L"Shaders/DeferredLighting.hlsl");
    if (!m_lightPerm) { SE_LOG_ERROR("DeferredPipeline: DeferredLighting.hlsl failed"); return false; }

    // Input layout matches Basic.hlsl vertex format
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    HRESULT hr = device->CreateInputLayout(layout, 5,
        m_geomPerm->vsBlob->GetBufferPointer(), m_geomPerm->vsBlob->GetBufferSize(),
        m_geomLayout.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("DeferredPipeline: input layout failed"); return false; }

    // Sampler (same anisotropic as ForwardPipeline)
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter        = D3D11_FILTER_ANISOTROPIC;
    sd.MaxAnisotropy = 16;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxLOD   = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&sd, m_sampler.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("DeferredPipeline: sampler failed"); return false; }

    if (!m_transformCB.Create(device)) return false;
    if (!m_materialCB.Create(device))  return false;
    if (!m_deferredCB.Create(device))  return false;
    if (!m_quad.Init(device, shaders)) return false;

    SE_LOG_INFO("DeferredPipeline initialised");
    return true;
}

void DeferredPipeline::Shutdown()
{
    m_quad.Shutdown();
    m_sampler.Reset();
    m_geomLayout.Reset();
    m_geomPerm  = nullptr;
    m_lightPerm = nullptr;
}

void DeferredPipeline::BeginGeometryPass(ID3D11DeviceContext* ctx, GBuffer& gb,
                                          XMMATRIX view, XMMATRIX proj)
{
    XMStoreFloat4x4(&m_view, view);
    XMStoreFloat4x4(&m_proj, proj);
    m_queue.clear();

    gb.Bind(ctx);

    ctx->IASetInputLayout(m_geomLayout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_geomPerm->vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_geomPerm->ps.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
}

void DeferredPipeline::SubmitMesh(const Mesh& mesh, XMMATRIX model,
                                   const std::vector<ForwardPipeline::SubMat>& subMats)
{
    RenderItem item;
    item.mesh    = &mesh;
    XMStoreFloat4x4(&item.model, model);
    item.subMats = &subMats;
    m_queue.push_back(item);
}

void DeferredPipeline::FlushGeometry(ID3D11DeviceContext* ctx)
{
    for (auto& item : m_queue)
    {
        TransformCBData tc;
        tc.model = item.model;
        tc.view  = m_view;
        tc.proj  = m_proj;
        m_transformCB.Update(ctx, tc);
        m_transformCB.BindVS(ctx, 0);

        const auto& subMats = *item.subMats;
        for (uint32_t s = 0; s < item.mesh->GetSubMeshCount(); ++s)
        {
            if (s < subMats.size())
            {
                if (subMats[s].albedo)    subMats[s].albedo->BindPS(ctx, 0);
                if (subMats[s].roughness) subMats[s].roughness->BindPS(ctx, 1);
                if (subMats[s].normal)    subMats[s].normal->BindPS(ctx, 2);
            }
            item.mesh->DrawSubMesh(ctx, s);
        }
    }
    m_queue.clear();
}

void DeferredPipeline::EndGeometryPass(ID3D11DeviceContext* ctx, GBuffer& gb)
{
    gb.Unbind(ctx);
}

void DeferredPipeline::SetMaterialParams(ID3D11DeviceContext* ctx,
                                          XMFLOAT3 albedoTint,
                                          float roughnessScale,
                                          float metallic)
{
    MaterialCBData mc;
    mc.albedoTint     = albedoTint;
    mc.roughnessScale = roughnessScale;
    mc.metallic       = metallic;
    mc.unlit          = 0.0f;
    mc.debugShadow    = 0.0f;
    mc._pad           = 0.0f;
    m_materialCB.Update(ctx, mc);
    m_materialCB.BindPS(ctx, 3);
}

void DeferredPipeline::LightingPass(ID3D11DeviceContext* ctx, GBuffer& gb,
                                     RenderTarget& sceneRT,
                                     LightEnvironment& lights,
                                     ShadowMap& shadow,
                                     XMFLOAT3 cameraPos,
                                     XMMATRIX viewProj)
{
    // Bind scene RT as output
    const float clearBlack[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    sceneRT.Begin(ctx, clearBlack);

    // Bind G-buffer as SRVs (t0-t3)
    gb.BindForLighting(ctx);

    // Bind shadow map to t4 (shifted from forward's t3)
    ID3D11ShaderResourceView* shadowSRV = shadow.GetSRV();
    ctx->PSSetShaderResources(4, 1, &shadowSRV);

    // Bind shadow comparison sampler to s1
    ID3D11SamplerState* shadowSamp = shadow.GetSampler();
    ctx->PSSetSamplers(1, 1, &shadowSamp);

    // Bind light constant buffers (b1, b2) — reuse LightEnvironment
    lights.BindPS(ctx, cameraPos, shadow.GetLightViewProj());

    // Bind deferred CB (b0) with InvViewProj + screen size
    DeferredCBData dc;
    XMMATRIX invVP = XMMatrixInverse(nullptr, viewProj);
    XMStoreFloat4x4(&dc.invViewProj, invVP);
    dc.screenW = static_cast<float>(gb.GetWidth());
    dc.screenH = static_cast<float>(gb.GetHeight());
    dc._pad[0] = dc._pad[1] = 0.0f;
    m_deferredCB.Update(ctx, dc);
    m_deferredCB.BindPS(ctx, 0);
    m_deferredCB.BindVS(ctx, 0);

    // Set lighting pass shaders and draw fullscreen quad
    ctx->VSSetShader(m_lightPerm->vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_lightPerm->ps.Get(), nullptr, 0);

    // Use point sampler for exact texel reads from G-buffer
    m_quad.Draw(ctx);

    // Unbind
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(4, 1, &nullSRV);
    gb.UnbindLighting(ctx);

    sceneRT.End(ctx);
}

} // namespace SE
