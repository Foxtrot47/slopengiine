#include "Engine/Renderer/SSR.h"
#include "Engine/Core/Logger.h"

using namespace DirectX;

namespace SE {

bool SSR::Init(ID3D11Device* device, ShaderLibrary& shaders,
               uint32_t width, uint32_t height)
{
    m_width  = width;
    m_height = height;

    // SSR result at full resolution for quality
    if (!m_ssrRT.Init(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT))
        return false;

    // Compile shaders
    m_ssrPerm = shaders.Get(L"Shaders/SSR.hlsl");
    if (!m_ssrPerm) { SE_LOG_ERROR("SSR: failed to compile SSR.hlsl"); return false; }

    m_compositePerm = shaders.Get(L"Shaders/SSRComposite.hlsl");
    if (!m_compositePerm) { SE_LOG_ERROR("SSR: failed to compile SSRComposite.hlsl"); return false; }

    if (!m_cb.Create(device)) return false;
    if (!m_quad.Init(device, shaders)) return false;

    // Alpha blend state for composite pass
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable           = TRUE;
    bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    HRESULT hr = device->CreateBlendState(&bd, m_alphaBlend.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("SSR: blend state failed"); return false; }

    // Linear sampler for SSR texture sampling
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = device->CreateSamplerState(&sd, m_linearSampler.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("SSR: sampler failed"); return false; }

    SE_LOG_INFO("SSR initialised — %ux%u", width, height);
    return true;
}

void SSR::Resize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    m_width  = width;
    m_height = height;
    m_ssrRT.Shutdown();
    m_ssrRT.Init(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
}

void SSR::Shutdown()
{
    m_ssrRT.Shutdown();
    m_alphaBlend.Reset();
    m_linearSampler.Reset();
}

void SSR::Apply(ID3D11DeviceContext* ctx,
                RenderTarget& hdrRT,
                ID3D11ShaderResourceView* depthSRV,
                ID3D11ShaderResourceView* normalSRV,
                XMMATRIX proj)
{
    if (!enabled) return;

    // Compute inverse projection
    XMMATRIX invProj = XMMatrixInverse(nullptr, proj);

    // Update constant buffer
    SSRCBData cbData = {};
    XMStoreFloat4x4(&cbData.invProj, invProj);
    XMStoreFloat4x4(&cbData.proj, proj);
    cbData.screenWidth  = (float)m_width;
    cbData.screenHeight = (float)m_height;
    cbData.maxDistance  = maxDistance;
    cbData.thickness    = thickness;
    cbData._unused0    = 0.0f;
    cbData.maxSteps     = maxSteps;
    cbData.binarySteps  = binarySteps;
    cbData.intensity    = intensity;
    cbData._unused1    = 0;
    m_cb.Update(ctx, cbData);

    // --- Pass 1: Ray march into half-res SSR target ---
    {
        const float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_ssrRT.Begin(ctx, black);

        ctx->PSSetShader(m_ssrPerm->ps.Get(), nullptr, 0);
        ctx->VSSetShader(m_ssrPerm->vs.Get(), nullptr, 0);

        // Bind inputs: t0=scene color, t1=depth, t2=normals
        ID3D11ShaderResourceView* srvs[3] = { hdrRT.GetSRV(), depthSRV, normalSRV };
        ctx->PSSetShaderResources(0, 3, srvs);
        ctx->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());

        m_cb.BindPS(ctx, 0);

        m_quad.DrawGeometryOnly(ctx);

        // Unbind scene SRVs
        ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
        ctx->PSSetShaderResources(0, 3, nullSRVs);

        m_ssrRT.End(ctx);
    }

    // --- Pass 2: Composite SSR into HDR buffer with alpha blending ---
    {
        // Bind HDR RT as render target (no depth, no clear)
        ID3D11RenderTargetView* rtv = hdrRT.GetRTV();
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT vp = {};
        vp.Width    = (float)hdrRT.GetWidth();
        vp.Height   = (float)hdrRT.GetHeight();
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);

        float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ctx->OMSetBlendState(m_alphaBlend.Get(), blendFactor, 0xFFFFFFFF);

        // Bind SSR result to t0
        m_ssrRT.BindPS(ctx, 0);
        ctx->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());

        m_quad.Draw(ctx, m_compositePerm);

        m_ssrRT.UnbindPS(ctx, 0);

        // Restore default blend state
        ctx->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    }
}

} // namespace SE
