#include "Engine/Renderer/RenderTarget.h"
#include "Engine/Core/Logger.h"

namespace SE {

bool RenderTarget::Init(ID3D11Device* device, uint32_t width, uint32_t height,
                         DXGI_FORMAT format, bool withDepth)
{
    m_width  = width;
    m_height = height;

    // Color texture (SRV + RTV)
    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = format;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&td, nullptr, m_tex.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("RenderTarget: CreateTexture2D failed (0x%08X)", hr); return false; }

    hr = device->CreateRenderTargetView(m_tex.Get(), nullptr, m_rtv.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("RenderTarget: CreateRTV failed (0x%08X)", hr); return false; }

    hr = device->CreateShaderResourceView(m_tex.Get(), nullptr, m_srv.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("RenderTarget: CreateSRV failed (0x%08X)", hr); return false; }

    // Optional depth buffer
    if (withDepth)
    {
        D3D11_TEXTURE2D_DESC dd = {};
        dd.Width            = width;
        dd.Height           = height;
        dd.MipLevels        = 1;
        dd.ArraySize        = 1;
        dd.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dd.SampleDesc.Count = 1;
        dd.Usage            = D3D11_USAGE_DEFAULT;
        dd.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

        hr = device->CreateTexture2D(&dd, nullptr, m_depthTex.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("RenderTarget: depth CreateTexture2D failed (0x%08X)", hr); return false; }

        hr = device->CreateDepthStencilView(m_depthTex.Get(), nullptr, m_dsv.GetAddressOf());
        if (FAILED(hr)) { SE_LOG_ERROR("RenderTarget: CreateDSV failed (0x%08X)", hr); return false; }
    }

    SE_LOG_INFO("RenderTarget initialised — %ux%u, format=%u, depth=%s",
        width, height, (unsigned)format, withDepth ? "yes" : "no");
    return true;
}

void RenderTarget::Shutdown()
{
    m_dsv.Reset();
    m_depthTex.Reset();
    m_srv.Reset();
    m_rtv.Reset();
    m_tex.Reset();
    m_width = m_height = 0;
}

void RenderTarget::Begin(ID3D11DeviceContext* ctx, const float clearColor[4])
{
    // Save current state
    UINT numVP = 1;
    ctx->RSGetViewports(&numVP, &m_savedVP);
    ctx->OMGetRenderTargets(1, &m_savedRtv, &m_savedDsv);

    // Bind our target
    ID3D11RenderTargetView* rtv = m_rtv.Get();
    ctx->OMSetRenderTargets(1, &rtv, m_dsv.Get());

    if (clearColor)
        ctx->ClearRenderTargetView(m_rtv.Get(), clearColor);
    if (m_dsv)
        ctx->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(m_width);
    vp.Height   = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

void RenderTarget::End(ID3D11DeviceContext* ctx)
{
    // Restore previous state
    ctx->OMSetRenderTargets(1, &m_savedRtv, m_savedDsv);
    ctx->RSSetViewports(1, &m_savedVP);

    // Release the refs OMGetRenderTargets gave us
    if (m_savedRtv) { m_savedRtv->Release(); m_savedRtv = nullptr; }
    if (m_savedDsv) { m_savedDsv->Release(); m_savedDsv = nullptr; }
}

void RenderTarget::BindPS(ID3D11DeviceContext* ctx, UINT slot) const
{
    ID3D11ShaderResourceView* srv = m_srv.Get();
    ctx->PSSetShaderResources(slot, 1, &srv);
}

void RenderTarget::UnbindPS(ID3D11DeviceContext* ctx, UINT slot) const
{
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(slot, 1, &none);
}

} // namespace SE
