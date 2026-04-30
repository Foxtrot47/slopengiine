#include "Engine/Renderer/Bloom.h"
#include "Engine/Core/Logger.h"
#include <algorithm>

namespace SE {

bool Bloom::Init(ID3D11Device* device, ShaderLibrary& shaders,
                 uint32_t width, uint32_t height)
{
    m_extractPerm   = shaders.Get(L"Shaders/Bloom.hlsl", {{"PASS_EXTRACT", "1"}});
    m_downPerm      = shaders.Get(L"Shaders/Bloom.hlsl", {{"PASS_DOWN", "1"}});
    m_upPerm        = shaders.Get(L"Shaders/Bloom.hlsl", {{"PASS_UP", "1"}});
    m_compositePerm = shaders.Get(L"Shaders/Bloom.hlsl", {{"PASS_COMPOSITE", "1"}});

    if (!m_extractPerm || !m_downPerm || !m_upPerm || !m_compositePerm)
    {
        SE_LOG_ERROR("Bloom: failed to load shader permutations");
        return false;
    }

    if (!m_cb.Create(device)) return false;
    if (!m_quad.Init(device, shaders)) return false;

    // Linear-clamp sampler for blur
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD   = D3D11_FLOAT32_MAX;
    HRESULT hr = device->CreateSamplerState(&sd, m_linearSampler.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("Bloom: sampler creation failed"); return false; }

    RebuildMips(device, width, height);
    return true;
}

void Bloom::Shutdown()
{
    for (int i = 0; i < k_maxMips; ++i) m_mips[i].Shutdown();
    m_compositeRT.Shutdown();
    m_linearSampler.Reset();
}

void Bloom::Resize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    if (width == m_width && height == m_height) return;
    RebuildMips(device, width, height);
}

void Bloom::RebuildMips(ID3D11Device* device, uint32_t w, uint32_t h)
{
    m_width = w;
    m_height = h;

    for (int i = 0; i < k_maxMips; ++i) m_mips[i].Shutdown();
    m_compositeRT.Shutdown();

    // Build a mip chain at half-res, quarter-res, ... (minimum 4x4)
    m_numMips = 0;
    uint32_t mw = w / 2, mh = h / 2;
    for (int i = 0; i < k_maxMips && mw >= 4 && mh >= 4; ++i)
    {
        m_mips[i].Init(device, mw, mh, DXGI_FORMAT_R11G11B10_FLOAT);
        m_numMips++;
        mw /= 2;
        mh /= 2;
    }

    // Full-res composite RT
    m_compositeRT.Init(device, w, h, DXGI_FORMAT_R11G11B10_FLOAT);

    SE_LOG_INFO("Bloom: %d mip levels, base %ux%u", m_numMips, w, h);
}

// Helper: bind VS/PS from permutation, bind our linear sampler, draw fullscreen quad.
void Bloom::DrawPass(ID3D11DeviceContext* ctx, const ShaderPermutation* perm)
{
    ctx->VSSetShader(perm->vs.Get(), nullptr, 0);
    ctx->PSSetShader(perm->ps.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());
    m_quad.DrawGeometryOnly(ctx);
}

ID3D11ShaderResourceView* Bloom::Apply(ID3D11DeviceContext* ctx,
                                        ID3D11ShaderResourceView* hdrSceneSRV,
                                        uint32_t sceneW, uint32_t sceneH)
{
    if (!enabled || m_numMips == 0)
        return hdrSceneSRV;

    static const float black[4] = { 0, 0, 0, 1 };

    // --- Pass 1: Extract bright pixels into mips[0] ---
    {
        CBData cb = { 1.0f / m_mips[0].GetWidth(), 1.0f / m_mips[0].GetHeight(),
                      threshold, intensity };
        m_cb.Update(ctx, cb);
        m_cb.BindPS(ctx, 0);

        m_mips[0].Begin(ctx, black);
        ctx->PSSetShaderResources(0, 1, &hdrSceneSRV);
        DrawPass(ctx, m_extractPerm);
        m_mips[0].End(ctx);
    }

    // --- Pass 2: Downsample chain ---
    for (int i = 1; i < m_numMips; ++i)
    {
        CBData cb = { 1.0f / m_mips[i].GetWidth(), 1.0f / m_mips[i].GetHeight(),
                      threshold, intensity };
        m_cb.Update(ctx, cb);
        m_cb.BindPS(ctx, 0);

        m_mips[i].Begin(ctx, black);
        m_mips[i - 1].BindPS(ctx, 0);
        DrawPass(ctx, m_downPerm);
        m_mips[i].End(ctx);
        m_mips[i - 1].UnbindPS(ctx, 0);
    }

    // --- Pass 3: Upsample chain (back up the mip chain) ---
    for (int i = m_numMips - 2; i >= 0; --i)
    {
        CBData cb = { 1.0f / m_mips[i].GetWidth(), 1.0f / m_mips[i].GetHeight(),
                      threshold, intensity };
        m_cb.Update(ctx, cb);
        m_cb.BindPS(ctx, 0);

        m_mips[i].Begin(ctx, black);
        m_mips[i + 1].BindPS(ctx, 0);
        DrawPass(ctx, m_upPerm);
        m_mips[i].End(ctx);
        m_mips[i + 1].UnbindPS(ctx, 0);
    }

    // --- Pass 4: Composite (scene + bloom) ---
    {
        CBData cb = { 1.0f / sceneW, 1.0f / sceneH, threshold, intensity };
        m_cb.Update(ctx, cb);
        m_cb.BindPS(ctx, 0);

        m_compositeRT.Begin(ctx, black);
        ctx->PSSetShaderResources(0, 1, &hdrSceneSRV);
        ID3D11ShaderResourceView* bloomSRV = m_mips[0].GetSRV();
        ctx->PSSetShaderResources(1, 1, &bloomSRV);
        DrawPass(ctx, m_compositePerm);
        m_compositeRT.End(ctx);

        // Unbind
        ID3D11ShaderResourceView* nulls[2] = {};
        ctx->PSSetShaderResources(0, 2, nulls);
    }

    return m_compositeRT.GetSRV();
}

} // namespace SE
