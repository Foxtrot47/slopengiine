#include "Engine/Renderer/SSAO.h"
#include "Engine/Core/Logger.h"
#include <random>
#include <algorithm>

using namespace DirectX;

namespace SE {

void SSAO::GenerateKernel()
{
    std::default_random_engine rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        // Random point in hemisphere (tangent space: z is up)
        float x = dist(rng) * 2.0f - 1.0f;
        float y = dist(rng) * 2.0f - 1.0f;
        float z = dist(rng); // hemisphere: z in [0,1]

        XMVECTOR v = XMVector3Normalize(XMVectorSet(x, y, z, 0.0f));

        // Scale so more samples cluster near the origin
        float scale = (float)i / (float)KERNEL_SIZE;
        scale = 0.1f + scale * scale * 0.9f; // lerp(0.1, 1.0, scale^2)
        v = XMVectorScale(v, scale);

        XMStoreFloat4(&m_kernel[i], v);
        m_kernel[i].w = 0.0f;
    }
}

void SSAO::CreateNoiseTexture(ID3D11Device* device)
{
    // 4x4 texture of random rotation vectors (in tangent-space XY plane)
    std::default_random_engine rng(7);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    struct PixelRGBA8 { uint8_t r, g, b, a; };
    PixelRGBA8 pixels[16];

    for (int i = 0; i < 16; ++i)
    {
        // Random vector in XY plane, encoded as [0,1] for unorm texture
        float x = dist(rng) * 2.0f - 1.0f;
        float y = dist(rng) * 2.0f - 1.0f;
        float len = sqrtf(x * x + y * y);
        if (len > 0.001f) { x /= len; y /= len; }
        pixels[i].r = (uint8_t)((x * 0.5f + 0.5f) * 255.0f);
        pixels[i].g = (uint8_t)((y * 0.5f + 0.5f) * 255.0f);
        pixels[i].b = 128; // z = 0 encoded
        pixels[i].a = 255;
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width     = 4;
    td.Height    = 4;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage     = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem     = pixels;
    initData.SysMemPitch = 4 * sizeof(PixelRGBA8);

    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&td, &initData, tex.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("SSAO: noise texture creation failed"); return; }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(tex.Get(), &srvDesc, m_noiseSRV.GetAddressOf());
}

bool SSAO::Init(ID3D11Device* device, ShaderLibrary& shaders,
                uint32_t width, uint32_t height)
{
    m_width  = width;
    m_height = height;

    // Single-channel AO render targets (R8_UNORM is sufficient)
    if (!m_aoRT.Init(device, width, height, DXGI_FORMAT_R8_UNORM))
        return false;
    if (!m_blurRT.Init(device, width, height, DXGI_FORMAT_R8_UNORM))
        return false;

    m_ssaoPerm = shaders.Get(L"Shaders/SSAO.hlsl");
    if (!m_ssaoPerm) { SE_LOG_ERROR("SSAO: failed to compile SSAO.hlsl"); return false; }

    m_blurPerm = shaders.Get(L"Shaders/SSAOBlur.hlsl");
    if (!m_blurPerm) { SE_LOG_ERROR("SSAO: failed to compile SSAOBlur.hlsl"); return false; }

    m_applyPerm = shaders.Get(L"Shaders/SSAOApply.hlsl");
    if (!m_applyPerm) { SE_LOG_ERROR("SSAO: failed to compile SSAOApply.hlsl"); return false; }

    if (!m_cb.Create(device)) return false;
    if (!m_blurCB.Create(device)) return false;
    if (!m_quad.Init(device, shaders)) return false;

    GenerateKernel();
    CreateNoiseTexture(device);

    // Clamp sampler for depth/AO reads
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    HRESULT hr = device->CreateSamplerState(&sd, m_clampSampler.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("SSAO: clamp sampler failed"); return false; }

    // Wrap sampler for noise texture tiling
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = device->CreateSamplerState(&sd, m_wrapSampler.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("SSAO: wrap sampler failed"); return false; }

    // Multiply blend state: Final = DestColor * SrcColor
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable           = TRUE;
    bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].DestBlend             = D3D11_BLEND_SRC_COLOR;
    bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = device->CreateBlendState(&bd, m_multiplyBlend.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("SSAO: blend state failed"); return false; }

    SE_LOG_INFO("SSAO initialised — %ux%u, %d samples", width, height, KERNEL_SIZE);
    return true;
}

void SSAO::Resize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    m_width  = width;
    m_height = height;
    m_aoRT.Shutdown();
    m_blurRT.Shutdown();
    m_aoRT.Init(device, width, height, DXGI_FORMAT_R8_UNORM);
    m_blurRT.Init(device, width, height, DXGI_FORMAT_R8_UNORM);
}

void SSAO::Shutdown()
{
    m_aoRT.Shutdown();
    m_blurRT.Shutdown();
    m_noiseSRV.Reset();
    m_clampSampler.Reset();
    m_wrapSampler.Reset();
    m_multiplyBlend.Reset();
}

void SSAO::Apply(ID3D11DeviceContext* ctx,
                 RenderTarget& hdrRT,
                 ID3D11ShaderResourceView* depthSRV,
                 ID3D11ShaderResourceView* normalSRV,
                 XMMATRIX proj)
{
    if (!enabled) return;

    XMMATRIX invProj = XMMatrixInverse(nullptr, proj);

    // Update SSAO constant buffer
    SSAOCBData cbData = {};
    XMStoreFloat4x4(&cbData.invProj, invProj);
    XMStoreFloat4x4(&cbData.proj, proj);
    memcpy(cbData.samples, m_kernel, sizeof(m_kernel));
    cbData.screenWidth  = (float)m_width;
    cbData.screenHeight = (float)m_height;
    cbData.noiseScaleX  = (float)m_width / 4.0f;
    cbData.noiseScaleY  = (float)m_height / 4.0f;
    cbData.radius       = radius;
    cbData.bias         = bias;
    cbData.power        = power;
    cbData._pad         = 0.0f;
    m_cb.Update(ctx, cbData);

    // --- Pass 1: Compute raw AO ---
    {
        const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_aoRT.Begin(ctx, white);

        ctx->VSSetShader(m_ssaoPerm->vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_ssaoPerm->ps.Get(), nullptr, 0);

        ID3D11ShaderResourceView* srvs[3] = { depthSRV, normalSRV, m_noiseSRV.Get() };
        ctx->PSSetShaderResources(0, 3, srvs);

        ID3D11SamplerState* samplers[2] = { m_clampSampler.Get(), m_wrapSampler.Get() };
        ctx->PSSetSamplers(0, 2, samplers);

        m_cb.BindPS(ctx, 0);
        m_quad.DrawGeometryOnly(ctx);

        ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
        ctx->PSSetShaderResources(0, 3, nullSRVs);
        m_aoRT.End(ctx);
    }

    // --- Pass 2: Horizontal blur ---
    {
        const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_blurRT.Begin(ctx, white);

        ctx->VSSetShader(m_blurPerm->vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_blurPerm->ps.Get(), nullptr, 0);

        BlurCBData blurData = {};
        blurData.texelSizeX = 1.0f / (float)m_width;
        blurData.texelSizeY = 1.0f / (float)m_height;
        blurData.dirX = 1.0f;
        blurData.dirY = 0.0f;
        m_blurCB.Update(ctx, blurData);
        m_blurCB.BindPS(ctx, 0);

        ID3D11ShaderResourceView* srvs[2] = { m_aoRT.GetSRV(), depthSRV };
        ctx->PSSetShaderResources(0, 2, srvs);
        ctx->PSSetSamplers(0, 1, m_clampSampler.GetAddressOf());

        m_quad.DrawGeometryOnly(ctx);

        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        ctx->PSSetShaderResources(0, 2, nullSRVs);
        m_blurRT.End(ctx);
    }

    // --- Pass 3: Vertical blur (write back into m_aoRT) ---
    {
        const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_aoRT.Begin(ctx, white);

        ctx->VSSetShader(m_blurPerm->vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_blurPerm->ps.Get(), nullptr, 0);

        BlurCBData blurData = {};
        blurData.texelSizeX = 1.0f / (float)m_width;
        blurData.texelSizeY = 1.0f / (float)m_height;
        blurData.dirX = 0.0f;
        blurData.dirY = 1.0f;
        m_blurCB.Update(ctx, blurData);
        m_blurCB.BindPS(ctx, 0);

        ID3D11ShaderResourceView* srvs[2] = { m_blurRT.GetSRV(), depthSRV };
        ctx->PSSetShaderResources(0, 2, srvs);
        ctx->PSSetSamplers(0, 1, m_clampSampler.GetAddressOf());

        m_quad.DrawGeometryOnly(ctx);

        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        ctx->PSSetShaderResources(0, 2, nullSRVs);
        m_aoRT.End(ctx);
    }

    // --- Pass 4: Multiply AO into HDR buffer using blend state ---
    {
        ID3D11RenderTargetView* rtv = hdrRT.GetRTV();
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT vp = {};
        vp.Width    = (float)hdrRT.GetWidth();
        vp.Height   = (float)hdrRT.GetHeight();
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);

        // Multiply blend: Final = DestColor * SrcColor (AO)
        float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ctx->OMSetBlendState(m_multiplyBlend.Get(), blendFactor, 0xFFFFFFFF);

        ctx->VSSetShader(m_applyPerm->vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_applyPerm->ps.Get(), nullptr, 0);

        ID3D11ShaderResourceView* srv = m_aoRT.GetSRV();
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->PSSetSamplers(0, 1, m_clampSampler.GetAddressOf());

        m_quad.DrawGeometryOnly(ctx);

        ID3D11ShaderResourceView* nullSRV = nullptr;
        ctx->PSSetShaderResources(0, 1, &nullSRV);

        // Restore default blend state
        ctx->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    }
}

} // namespace SE
