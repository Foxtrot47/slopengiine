#include "Engine/Renderer/GBuffer.h"
#include "Engine/Core/Logger.h"

namespace SE {

static const DXGI_FORMAT k_colorFormats[3] = {
    DXGI_FORMAT_R8G8B8A8_UNORM,      // RT0: albedo.rgb + AO(a)
    DXGI_FORMAT_R16G16B16A16_FLOAT,   // RT1: world normal.xyz + unused(a)
    DXGI_FORMAT_R8G8B8A8_UNORM,       // RT2: roughness(r) + metallic(g) + reserved(ba)
};

bool GBuffer::Init(ID3D11Device* device, uint32_t width, uint32_t height)
{
    m_width  = width;
    m_height = height;

    for (int i = 0; i < k_numColorTargets; ++i)
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = width;
        td.Height           = height;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = k_colorFormats[i];
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_DEFAULT;
        td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device->CreateTexture2D(&td, nullptr, m_colorTex[i].GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("GBuffer: color tex %d failed (0x%08X)", i, hr); return false; }

        hr = device->CreateRenderTargetView(m_colorTex[i].Get(), nullptr, m_colorRTV[i].GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("GBuffer: color RTV %d failed", i); return false; }

        hr = device->CreateShaderResourceView(m_colorTex[i].Get(), nullptr, m_colorSRV[i].GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("GBuffer: color SRV %d failed", i); return false; }
    }

    // Depth: typeless so we can create both DSV and SRV
    {
        D3D11_TEXTURE2D_DESC dd = {};
        dd.Width            = width;
        dd.Height           = height;
        dd.MipLevels        = 1;
        dd.ArraySize        = 1;
        dd.Format           = DXGI_FORMAT_R32_TYPELESS;
        dd.SampleDesc.Count = 1;
        dd.Usage            = D3D11_USAGE_DEFAULT;
        dd.BindFlags        = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device->CreateTexture2D(&dd, nullptr, m_depthTex.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("GBuffer: depth tex failed (0x%08X)", hr); return false; }

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format             = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = device->CreateDepthStencilView(m_depthTex.Get(), &dsvDesc, m_dsv.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("GBuffer: DSV failed"); return false; }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                    = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels       = 1;
        hr = device->CreateShaderResourceView(m_depthTex.Get(), &srvDesc, m_depthSRV.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("GBuffer: depth SRV failed"); return false; }
    }

    SE_LOG_INFO("GBuffer initialised — %ux%u, 3 MRTs + D32 depth", width, height);
    return true;
}

void GBuffer::Shutdown()
{
    m_depthSRV.Reset();
    m_dsv.Reset();
    m_depthTex.Reset();
    for (int i = 0; i < k_numColorTargets; ++i)
    {
        m_colorSRV[i].Reset();
        m_colorRTV[i].Reset();
        m_colorTex[i].Reset();
    }
    m_width = m_height = 0;
}

void GBuffer::Bind(ID3D11DeviceContext* ctx)
{
    // Save current state
    m_savedNumVPs = 1;
    ctx->RSGetViewports(&m_savedNumVPs, &m_savedVP);
    ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                            m_savedRTVs, &m_savedDSV);

    // Bind MRT
    ID3D11RenderTargetView* rtvs[k_numColorTargets] = {
        m_colorRTV[0].Get(), m_colorRTV[1].Get(), m_colorRTV[2].Get()
    };
    ctx->OMSetRenderTargets(k_numColorTargets, rtvs, m_dsv.Get());

    // Clear all targets
    const float clearAlbedo[4]    = { 0.0f, 0.0f, 0.0f, 1.0f };
    const float clearNormal[4]    = { 0.0f, 0.0f, 0.0f, 0.0f };
    const float clearMaterial[4]  = { 1.0f, 0.0f, 0.0f, 0.0f };
    ctx->ClearRenderTargetView(m_colorRTV[0].Get(), clearAlbedo);
    ctx->ClearRenderTargetView(m_colorRTV[1].Get(), clearNormal);
    ctx->ClearRenderTargetView(m_colorRTV[2].Get(), clearMaterial);
    ctx->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(m_width);
    vp.Height   = static_cast<float>(m_height);
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

void GBuffer::Unbind(ID3D11DeviceContext* ctx)
{
    ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                            m_savedRTVs, m_savedDSV);
    ctx->RSSetViewports(1, &m_savedVP);

    for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        if (m_savedRTVs[i]) { m_savedRTVs[i]->Release(); m_savedRTVs[i] = nullptr; }
    }
    if (m_savedDSV) { m_savedDSV->Release(); m_savedDSV = nullptr; }
}

void GBuffer::BindForLighting(ID3D11DeviceContext* ctx) const
{
    ID3D11ShaderResourceView* srvs[4] = {
        m_colorSRV[0].Get(),   // t0: albedo
        m_colorSRV[1].Get(),   // t1: normal
        m_colorSRV[2].Get(),   // t2: material
        m_depthSRV.Get()       // t3: depth
    };
    ctx->PSSetShaderResources(0, 4, srvs);
}

void GBuffer::UnbindLighting(ID3D11DeviceContext* ctx) const
{
    ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
    ctx->PSSetShaderResources(0, 4, nullSRVs);
}

void GBuffer::BindDepthForComposite(ID3D11DeviceContext* ctx,
                                     ID3D11RenderTargetView* sceneRTV) const
{
    // Unbind depth SRV first (can't be SRV and DSV simultaneously)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(3, 1, &nullSRV);

    ctx->OMSetRenderTargets(1, &sceneRTV, m_dsv.Get());
}

} // namespace SE
