#include "Engine/Renderer/IBLEnvironment.h"
#include "Engine/Core/Logger.h"

namespace SE {

bool IBLEnvironment::Init(ID3D11Device* device, ShaderLibrary& shaders)
{
    m_equirectPerm   = shaders.Get(L"Shaders/IBL.hlsl", {{"PASS_EQUIRECT", "1"}});
    m_irradiancePerm = shaders.Get(L"Shaders/IBL.hlsl", {{"PASS_IRRADIANCE", "1"}});
    m_prefilteredPerm = shaders.Get(L"Shaders/IBL.hlsl", {{"PASS_PREFILTER", "1"}});
    m_brdfPerm       = shaders.Get(L"Shaders/IBL.hlsl", {{"PASS_BRDF", "1"}});

    if (!m_equirectPerm || !m_irradiancePerm || !m_prefilteredPerm || !m_brdfPerm)
    {
        SE_LOG_ERROR("IBLEnvironment: failed to compile one or more shader permutations");
        return false;
    }

    if (!m_cb.Create(device)) return false;
    if (!m_quad.Init(device, shaders)) return false;

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD   = D3D11_FLOAT32_MAX;
    HRESULT hr = device->CreateSamplerState(&sd, m_linearSampler.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("IBL: sampler failed"); return false; }

    return true;
}

void IBLEnvironment::Shutdown()
{
    m_brdfLutRTV.Reset(); m_brdfLutSRV.Reset(); m_brdfLutTex.Reset();
    m_prefilteredSRV.Reset(); m_prefilteredTex.Reset();
    m_irradianceSRV.Reset(); m_irradianceTex.Reset();
    m_envCubeSRV.Reset(); m_envCubeTex.Reset();
    m_linearSampler.Reset();
    m_ready = false;
}

bool IBLEnvironment::CreateCubemap(ID3D11Device* device, uint32_t size, int mipLevels,
                                    ComPtr<ID3D11Texture2D>& outTex,
                                    ComPtr<ID3D11ShaderResourceView>& outSRV,
                                    bool generateMips)
{
    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = size;
    td.Height           = size;
    td.MipLevels        = mipLevels;
    td.ArraySize        = 6;
    td.Format           = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags        = D3D11_RESOURCE_MISC_TEXTURECUBE;
    if (generateMips)
        td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

    HRESULT hr = device->CreateTexture2D(&td, nullptr, outTex.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("IBL: CreateTexture2D cubemap failed 0x%08X", hr); return false; }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format                  = td.Format;
    srvd.ViewDimension           = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvd.TextureCube.MostDetailedMip = 0;
    srvd.TextureCube.MipLevels       = (mipLevels == 0) ? (UINT)-1 : mipLevels;

    hr = device->CreateShaderResourceView(outTex.Get(), &srvd, outSRV.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("IBL: CreateSRV cubemap failed 0x%08X", hr); return false; }

    return true;
}

ComPtr<ID3D11RenderTargetView> IBLEnvironment::CreateFaceRTV(ID3D11Device* device,
                                                               ID3D11Texture2D* tex,
                                                               int face, int mip)
{
    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
    rtvd.Format               = DXGI_FORMAT_R16G16B16A16_FLOAT;
    rtvd.ViewDimension        = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvd.Texture2DArray.MipSlice        = mip;
    rtvd.Texture2DArray.FirstArraySlice = face;
    rtvd.Texture2DArray.ArraySize       = 1;

    ComPtr<ID3D11RenderTargetView> rtv;
    HRESULT hr = device->CreateRenderTargetView(tex, &rtvd, rtv.GetAddressOf());
    if (FAILED(hr))
    {
        SE_LOG_ERROR("IBL: CreateFaceRTV failed face=%d mip=%d 0x%08X", face, mip, hr);
        return nullptr;
    }
    return rtv;
}

void IBLEnvironment::RenderFace(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv,
                                 uint32_t size, const ShaderPermutation* perm,
                                 int face, float roughness)
{
    const float black[4] = { 0, 0, 0, 1 };
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ctx->ClearRenderTargetView(rtv, black);

    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(size);
    vp.Height   = static_cast<float>(size);
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    CBData cb = { face, roughness, static_cast<float>(k_envCubeSize), 0.0f };
    m_cb.Update(ctx, cb);
    m_cb.BindPS(ctx, 0);

    ctx->VSSetShader(perm->vs.Get(), nullptr, 0);
    ctx->PSSetShader(perm->ps.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());
    m_quad.DrawGeometryOnly(ctx);
}

bool IBLEnvironment::Generate(ID3D11Device* device, ID3D11DeviceContext* ctx,
                               ID3D11ShaderResourceView* panoramaSRV)
{
    // Save current render state
    ID3D11RenderTargetView* savedRTV = nullptr;
    ID3D11DepthStencilView* savedDSV = nullptr;
    D3D11_VIEWPORT savedVP;
    UINT numVP = 1;
    ctx->OMGetRenderTargets(1, &savedRTV, &savedDSV);
    ctx->RSGetViewports(&numVP, &savedVP);

    // --- Step 1: Equirect → Cubemap (with full mip chain for prefilter LOD) ---
    if (!CreateCubemap(device, k_envCubeSize, 0, m_envCubeTex, m_envCubeSRV, true))
        return false;

    ctx->PSSetShaderResources(0, 1, &panoramaSRV);
    for (int face = 0; face < 6; ++face)
    {
        auto rtv = CreateFaceRTV(device, m_envCubeTex.Get(), face);
        if (!rtv) return false;
        RenderFace(ctx, rtv.Get(), k_envCubeSize, m_equirectPerm, face);
    }
    // Unbind panorama before generating mips
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSRV);
    // Generate full mip chain — critical for prefilter LOD sampling
    ctx->GenerateMips(m_envCubeSRV.Get());
    SE_LOG_INFO("IBL: environment cubemap %ux%u + mips generated", k_envCubeSize, k_envCubeSize);

    // --- Step 2: Irradiance convolution ---
    if (!CreateCubemap(device, k_irradianceSize, 1, m_irradianceTex, m_irradianceSRV))
        return false;

    ID3D11ShaderResourceView* envSRV = m_envCubeSRV.Get();
    ctx->PSSetShaderResources(0, 1, &envSRV);
    for (int face = 0; face < 6; ++face)
    {
        auto rtv = CreateFaceRTV(device, m_irradianceTex.Get(), face);
        if (!rtv) return false;
        RenderFace(ctx, rtv.Get(), k_irradianceSize, m_irradiancePerm, face);
    }
    SE_LOG_INFO("IBL: irradiance cubemap %ux%u generated", k_irradianceSize, k_irradianceSize);
    ctx->PSSetShaderResources(0, 1, &nullSRV);

    // --- Step 3: Pre-filtered specular ---
    if (!CreateCubemap(device, k_prefilteredSize, k_prefilteredMips,
                       m_prefilteredTex, m_prefilteredSRV))
        return false;

    ctx->PSSetShaderResources(0, 1, &envSRV);
    for (int mip = 0; mip < k_prefilteredMips; ++mip)
    {
        uint32_t mipSize = k_prefilteredSize >> mip;
        float roughness  = static_cast<float>(mip) / static_cast<float>(k_prefilteredMips - 1);
        for (int face = 0; face < 6; ++face)
        {
            auto rtv = CreateFaceRTV(device, m_prefilteredTex.Get(), face, mip);
            if (!rtv) return false;
            RenderFace(ctx, rtv.Get(), mipSize, m_prefilteredPerm, face, roughness);
        }
    }
    SE_LOG_INFO("IBL: pre-filtered specular %ux%u (%d mips) generated",
                k_prefilteredSize, k_prefilteredSize, k_prefilteredMips);
    ctx->PSSetShaderResources(0, 1, &nullSRV);

    // --- Step 4: BRDF LUT ---
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = k_brdfLutSize;
        td.Height           = k_brdfLutSize;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R16G16_FLOAT;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_DEFAULT;
        td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device->CreateTexture2D(&td, nullptr, m_brdfLutTex.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("IBL: BRDF LUT texture failed"); return false; }

        hr = device->CreateShaderResourceView(m_brdfLutTex.Get(), nullptr,
                                               m_brdfLutSRV.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("IBL: BRDF LUT SRV failed"); return false; }

        hr = device->CreateRenderTargetView(m_brdfLutTex.Get(), nullptr,
                                             m_brdfLutRTV.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("IBL: BRDF LUT RTV failed"); return false; }

        RenderFace(ctx, m_brdfLutRTV.Get(), k_brdfLutSize, m_brdfPerm, 0);
    }
    SE_LOG_INFO("IBL: BRDF LUT %ux%u generated", k_brdfLutSize, k_brdfLutSize);

    // Restore state
    ctx->OMSetRenderTargets(1, &savedRTV, savedDSV);
    ctx->RSSetViewports(1, &savedVP);
    if (savedRTV) savedRTV->Release();
    if (savedDSV) savedDSV->Release();

    // Unbind
    ctx->PSSetShaderResources(0, 1, &nullSRV);

    m_ready = true;
    SE_LOG_INFO("IBL: all textures generated successfully");
    return true;
}

} // namespace SE
