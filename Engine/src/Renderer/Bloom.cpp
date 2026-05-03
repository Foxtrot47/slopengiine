#include "Engine/Renderer/Bloom.h"
#include "Engine/Core/Logger.h"
#include <algorithm>

namespace SE {

bool Bloom::Init(ID3D11Device* device, ShaderLibrary& shaders,
                 uint32_t width, uint32_t height)
{
    m_thresholdPerm  = shaders.Get(L"Shaders/Bloom.hlsl", {{ "BLOOM_THRESHOLD",  "1" }});
    m_downsamplePerm = shaders.Get(L"Shaders/Bloom.hlsl", {{ "BLOOM_DOWNSAMPLE", "1" }});
    m_upsamplePerm   = shaders.Get(L"Shaders/Bloom.hlsl", {{ "BLOOM_UPSAMPLE",   "1" }});
    m_compositePerm  = shaders.Get(L"Shaders/Bloom.hlsl", {{ "BLOOM_COMPOSITE",  "1" }});

    if (!m_thresholdPerm || !m_downsamplePerm || !m_upsamplePerm || !m_compositePerm)
    {
        SE_LOG_ERROR("Bloom: failed to compile shader permutations");
        return false;
    }

    if (!m_cb.Create(device))                      return false;
    if (!m_quad.Init(device, shaders))             return false;

    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable           = TRUE;
    bd.RenderTarget[0].SrcBlend             = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend            = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp              = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha        = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha       = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha         = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    HRESULT hr = device->CreateBlendState(&bd, m_additiveBlend.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("Bloom: CreateBlendState failed (0x%08X)", hr); return false; }

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = device->CreateSamplerState(&sd, m_linearSampler.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("Bloom: CreateSamplerState failed (0x%08X)", hr); return false; }

    initRTs(device, width, height);

    SE_LOG_INFO("Bloom initialised — %u levels, %ux%u base", k_numLevels, width / 2, height / 2);
    return true;
}

void Bloom::Resize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    shutdownRTs();
    initRTs(device, width, height);
}

void Bloom::Shutdown()
{
    shutdownRTs();
    m_additiveBlend.Reset();
    m_linearSampler.Reset();
    m_thresholdPerm = m_downsamplePerm = m_upsamplePerm = m_compositePerm = nullptr;
}

void Bloom::Apply(ID3D11DeviceContext* ctx, RenderTarget& hdrRT)
{
    if (!enabled) return;

    auto unbind = [&]()
    {
        ID3D11ShaderResourceView* nullSRVs[2] = {};
        ctx->PSSetShaderResources(0, 2, nullSRVs);
    };

    // 1. Threshold: hdrRT → downChain[0]
    {
        updateCB(ctx, (float)hdrRT.GetWidth(), (float)hdrRT.GetHeight(),
                 threshold, intensity, scatter);
        bindDest(ctx, m_downChain[0]);
        ID3D11ShaderResourceView* srv = hdrRT.GetSRV();
        ctx->PSSetShaderResources(0, 1, &srv);
        draw(ctx, m_thresholdPerm);
        unbind();
    }

    // 2. Downsample chain: downChain[k] → downChain[k+1]
    for (int k = 0; k < k_numLevels - 1; ++k)
    {
        updateCB(ctx,
                 (float)m_downChain[k].GetWidth(), (float)m_downChain[k].GetHeight(),
                 0.0f, 0.0f, 0.0f);
        bindDest(ctx, m_downChain[k + 1]);
        ID3D11ShaderResourceView* srv = m_downChain[k].GetSRV();
        ctx->PSSetShaderResources(0, 1, &srv);
        draw(ctx, m_downsamplePerm);
        unbind();
    }

    // 3. First upsample: downChain[N-1] → upChain[N-2], blending with downChain[N-2]
    {
        const int tip = k_numLevels - 1;
        const int dst = k_numLevels - 2;
        updateCB(ctx,
                 (float)m_downChain[tip].GetWidth(), (float)m_downChain[tip].GetHeight(),
                 0.0f, 0.0f, scatter);
        bindDest(ctx, m_upChain[dst]);
        ID3D11ShaderResourceView* srvs[2] = {
            m_downChain[tip].GetSRV(),
            m_downChain[dst].GetSRV()
        };
        ctx->PSSetShaderResources(0, 2, srvs);
        draw(ctx, m_upsamplePerm);
        unbind();
    }

    // 4. Remaining upsample passes: upChain[k+1] → upChain[k], blending with downChain[k]
    for (int k = k_numLevels - 3; k >= 0; --k)
    {
        updateCB(ctx,
                 (float)m_upChain[k + 1].GetWidth(), (float)m_upChain[k + 1].GetHeight(),
                 0.0f, 0.0f, scatter);
        bindDest(ctx, m_upChain[k]);
        ID3D11ShaderResourceView* srvs[2] = {
            m_upChain[k + 1].GetSRV(),
            m_downChain[k].GetSRV()
        };
        ctx->PSSetShaderResources(0, 2, srvs);
        draw(ctx, m_upsamplePerm);
        unbind();
    }

    // 5. Composite: tent-upsample upChain[0] (half-res → full-res), additive blend into hdrRT
    {
        updateCB(ctx,
                 (float)m_upChain[0].GetWidth(), (float)m_upChain[0].GetHeight(),
                 0.0f, intensity, 0.0f);

        D3D11_VIEWPORT vp = {};
        vp.Width    = (float)hdrRT.GetWidth();
        vp.Height   = (float)hdrRT.GetHeight();
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);

        ID3D11RenderTargetView* hdrRTV = hdrRT.GetRTV();
        ctx->OMSetRenderTargets(1, &hdrRTV, nullptr);
        ctx->OMSetBlendState(m_additiveBlend.Get(), nullptr, 0xFFFFFFFF);

        ID3D11ShaderResourceView* srv = m_upChain[0].GetSRV();
        ctx->PSSetShaderResources(0, 1, &srv);
        draw(ctx, m_compositePerm);

        ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        unbind();
    }

    // Leave no RTV bound — caller (BindBackBuffer) sets its own.
    ID3D11RenderTargetView* nullRTV = nullptr;
    ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
}

// --- private helpers ---

void Bloom::initRTs(ID3D11Device* device, uint32_t w, uint32_t h)
{
    for (int k = 0; k < k_numLevels; ++k)
    {
        uint32_t lw = (std::max)(1u, w >> (k + 1));
        uint32_t lh = (std::max)(1u, h >> (k + 1));
        m_downChain[k].Init(device, lw, lh, DXGI_FORMAT_R16G16B16A16_FLOAT);
    }
    for (int k = 0; k < k_numLevels - 1; ++k)
    {
        uint32_t lw = (std::max)(1u, w >> (k + 1));
        uint32_t lh = (std::max)(1u, h >> (k + 1));
        m_upChain[k].Init(device, lw, lh, DXGI_FORMAT_R16G16B16A16_FLOAT);
    }
}

void Bloom::shutdownRTs()
{
    for (auto& rt : m_downChain) rt.Shutdown();
    for (auto& rt : m_upChain)   rt.Shutdown();
}

void Bloom::draw(ID3D11DeviceContext* ctx, const ShaderPermutation* perm)
{
    ctx->VSSetShader(perm->vs.Get(), nullptr, 0);
    ctx->PSSetShader(perm->ps.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());
    m_quad.DrawGeometryOnly(ctx);
}

void Bloom::updateCB(ID3D11DeviceContext* ctx,
                     float srcW, float srcH,
                     float thresh, float intens, float scat)
{
    BloomCB cb = {};
    cb.texelSizeX = 1.0f / srcW;
    cb.texelSizeY = 1.0f / srcH;
    cb.threshold  = thresh;
    cb.intensity  = intens;
    cb.scatter    = scat;
    m_cb.Update(ctx, cb);
    m_cb.BindPS(ctx, 0);
}

void Bloom::bindDest(ID3D11DeviceContext* ctx, RenderTarget& rt)
{
    D3D11_VIEWPORT vp = {};
    vp.Width    = (float)rt.GetWidth();
    vp.Height   = (float)rt.GetHeight();
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
    ID3D11RenderTargetView* rtv = rt.GetRTV();
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
}

} // namespace SE
