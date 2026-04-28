#include "Engine/Renderer/SSAO.h"
#include "Engine/Renderer/GBuffer.h"
#include "Engine/Core/Logger.h"
#include <random>
#include <cmath>

using namespace DirectX;

namespace SE {

// ---- Kernel & Noise generation ----

void SSAO::GenerateKernel()
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_real_distribution<float> distNeg(-1.0f, 1.0f);

    m_kernel.resize(k_maxKernelSize);
    for (int i = 0; i < k_maxKernelSize; ++i)
    {
        XMFLOAT3 v = { distNeg(rng), distNeg(rng), dist01(rng) };
        XMVECTOR sample = XMVector3Normalize(XMLoadFloat3(&v));
        sample = XMVectorScale(sample, dist01(rng));

        // Accelerating interpolation: bias samples toward origin
        float scale = static_cast<float>(i) / static_cast<float>(k_maxKernelSize);
        scale = 0.1f + 0.9f * scale * scale;
        sample = XMVectorScale(sample, scale);

        XMStoreFloat4(&m_kernel[i], sample);
        m_kernel[i].w = 0.0f;
    }
}

bool SSAO::CreateNoiseTexture(ID3D11Device* device)
{
    std::mt19937 rng(17);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    XMFLOAT4 noise[k_noiseSize * k_noiseSize];
    for (int i = 0; i < k_noiseSize * k_noiseSize; ++i)
    {
        float x = dist(rng);
        float y = dist(rng);
        float len = sqrtf(x * x + y * y);
        if (len < 0.001f) { x = 1.0f; y = 0.0f; len = 1.0f; }
        noise[i] = { x / len, y / len, 0.0f, 0.0f };
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = k_noiseSize;
    td.Height           = k_noiseSize;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem     = noise;
    sd.SysMemPitch = k_noiseSize * sizeof(XMFLOAT4);

    HRESULT hr = device->CreateTexture2D(&td, &sd, m_noiseTex.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("SSAO: noise texture failed (0x%08X)", hr); return false; }

    hr = device->CreateShaderResourceView(m_noiseTex.Get(), nullptr, m_noiseSRV.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("SSAO: noise SRV failed"); return false; }

    return true;
}

// ---- Init / Shutdown / Resize ----

bool SSAO::Init(ID3D11Device* device, ShaderLibrary& shaders, uint32_t w, uint32_t h)
{
    m_ssaoPerm = shaders.Get(L"Shaders/SSAO.hlsl");
    if (!m_ssaoPerm) { SE_LOG_ERROR("SSAO: SSAO.hlsl failed"); return false; }

    m_blurPerm = shaders.Get(L"Shaders/SSAOBlur.hlsl");
    if (!m_blurPerm) { SE_LOG_ERROR("SSAO: SSAOBlur.hlsl failed"); return false; }

    if (!m_aoRT.Init(device, w, h, DXGI_FORMAT_R8_UNORM))     return false;
    if (!m_blurRT.Init(device, w, h, DXGI_FORMAT_R8_UNORM))   return false;
    if (!m_quad.Init(device, shaders))                          return false;
    if (!m_paramsCB.Create(device))                             return false;
    if (!m_blurCB.Create(device))                               return false;
    if (!CreateNoiseTexture(device))                             return false;

    GenerateKernel();

    // Point-clamp sampler for G-buffer / AO reads
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        HRESULT hr = device->CreateSamplerState(&sd, m_pointClampSampler.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("SSAO: point clamp sampler failed"); return false; }
    }

    // Point-wrap sampler for noise texture tiling
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        HRESULT hr = device->CreateSamplerState(&sd, m_pointWrapSampler.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("SSAO: point wrap sampler failed"); return false; }
    }

    SE_LOG_INFO("SSAO initialised — %ux%u, kernel=%d", w, h, k_maxKernelSize);
    return true;
}

void SSAO::Shutdown()
{
    m_aoRT.Shutdown();
    m_blurRT.Shutdown();
    m_quad.Shutdown();
    m_noiseSRV.Reset();
    m_noiseTex.Reset();
    m_pointClampSampler.Reset();
    m_pointWrapSampler.Reset();
    m_ssaoPerm = nullptr;
    m_blurPerm = nullptr;
    m_kernel.clear();
}

void SSAO::Resize(ID3D11Device* device, uint32_t w, uint32_t h)
{
    if (m_aoRT.GetWidth() == w && m_aoRT.GetHeight() == h) return;
    m_aoRT.Shutdown();
    m_blurRT.Shutdown();
    m_aoRT.Init(device, w, h, DXGI_FORMAT_R8_UNORM);
    m_blurRT.Init(device, w, h, DXGI_FORMAT_R8_UNORM);
}

// ---- Render ----

void SSAO::Render(ID3D11DeviceContext* ctx, GBuffer& gb,
                   XMMATRIX view, XMMATRIX proj)
{
    if (!enabled) return;

    // --- 1. SSAO generation pass → m_aoRT ---
    {
        const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_aoRT.Begin(ctx, white);

        // Bind G-buffer depth (t0), normals (t1), noise (t2)
        ID3D11ShaderResourceView* srvs[3] = {
            gb.GetDepthSRV(),
            gb.GetNormalSRV(),
            m_noiseSRV.Get()
        };
        ctx->PSSetShaderResources(0, 3, srvs);

        // Samplers: s0 = point-clamp, s1 = point-wrap
        ID3D11SamplerState* samplers[2] = {
            m_pointClampSampler.Get(), m_pointWrapSampler.Get()
        };
        ctx->PSSetSamplers(0, 2, samplers);

        // Update + bind SSAO params CB
        SSAOParamsCB params = {};
        XMStoreFloat4x4(&params.projection, proj);
        XMStoreFloat4x4(&params.invProjection, XMMatrixInverse(nullptr, proj));
        XMStoreFloat4x4(&params.view, view);
        int clamped = (kernelSize < 1) ? 1 : (kernelSize > k_maxKernelSize ? k_maxKernelSize : kernelSize);
        for (int i = 0; i < k_maxKernelSize; ++i)
            params.samples[i] = m_kernel[i];
        params.screenSize = { static_cast<float>(gb.GetWidth()),
                              static_cast<float>(gb.GetHeight()) };
        params.noiseScale = { static_cast<float>(gb.GetWidth()) / k_noiseSize,
                              static_cast<float>(gb.GetHeight()) / k_noiseSize };
        params.radius     = radius;
        params.bias       = bias;
        params.intensity  = intensity;
        params.kernelSize = clamped;
        m_paramsCB.Update(ctx, params);
        m_paramsCB.BindPS(ctx, 0);
        m_paramsCB.BindVS(ctx, 0);

        // Set SSAO shaders and draw
        ctx->VSSetShader(m_ssaoPerm->vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_ssaoPerm->ps.Get(), nullptr, 0);
        m_quad.DrawGeometryOnly(ctx);

        // Unbind SRVs
        ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
        ctx->PSSetShaderResources(0, 3, nullSRVs);

        m_aoRT.End(ctx);
    }

    // --- 2. Horizontal bilateral blur: m_aoRT → m_blurRT ---
    {
        const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_blurRT.Begin(ctx, black);

        ID3D11ShaderResourceView* srvs[3] = {
            m_aoRT.GetSRV(),
            gb.GetDepthSRV(),
            gb.GetNormalSRV()
        };
        ctx->PSSetShaderResources(0, 3, srvs);
        ctx->PSSetSamplers(0, 1, m_pointClampSampler.GetAddressOf());

        BlurParamsCB blur = {};
        blur.blurDirection = { 1.0f / gb.GetWidth(), 0.0f };
        m_blurCB.Update(ctx, blur);
        m_blurCB.BindPS(ctx, 0);

        ctx->VSSetShader(m_blurPerm->vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_blurPerm->ps.Get(), nullptr, 0);
        m_quad.DrawGeometryOnly(ctx);

        ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
        ctx->PSSetShaderResources(0, 3, nullSRVs);

        m_blurRT.End(ctx);
    }

    // --- 3. Vertical bilateral blur: m_blurRT → m_aoRT ---
    {
        const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_aoRT.Begin(ctx, black);

        ID3D11ShaderResourceView* srvs[3] = {
            m_blurRT.GetSRV(),
            gb.GetDepthSRV(),
            gb.GetNormalSRV()
        };
        ctx->PSSetShaderResources(0, 3, srvs);
        ctx->PSSetSamplers(0, 1, m_pointClampSampler.GetAddressOf());

        BlurParamsCB blur = {};
        blur.blurDirection = { 0.0f, 1.0f / gb.GetHeight() };
        m_blurCB.Update(ctx, blur);
        m_blurCB.BindPS(ctx, 0);

        ctx->VSSetShader(m_blurPerm->vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_blurPerm->ps.Get(), nullptr, 0);
        m_quad.DrawGeometryOnly(ctx);

        ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
        ctx->PSSetShaderResources(0, 3, nullSRVs);

        m_aoRT.End(ctx);
    }
}

} // namespace SE
