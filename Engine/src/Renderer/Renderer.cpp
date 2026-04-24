#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/Logger.h"

namespace SE {

bool Renderer::Initialize(HWND hwnd, uint32_t width, uint32_t height)
{
    DXGI_SWAP_CHAIN_DESC sd               = {};
    sd.BufferDesc.Width                   = width;
    sd.BufferDesc.Height                  = height;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 0;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount                        = 2;
    sd.OutputWindow                       = hwnd;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL achievedLevel = {};

    UINT flags = 0;
#ifdef SE_DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &sd,
        &m_swapChain, &m_device, &achievedLevel, &m_context);

#ifdef SE_DEBUG
    if (FAILED(hr))
    {
        SE_LOG_WARN("D3D11 debug layer unavailable — retrying without it");
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            flags, featureLevels, ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION, &sd,
            &m_swapChain, &m_device, &achievedLevel, &m_context);
    }
#endif

    if (FAILED(hr))
    {
        SE_LOG_ERROR("D3D11CreateDeviceAndSwapChain failed: 0x%08X", hr);
        return false;
    }

    // ---- Render target view ----
    ComPtr<ID3D11Texture2D> backBuffer;
    SE_HR(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    SE_HR(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv));

    // ---- Depth/stencil texture + view ----
    D3D11_TEXTURE2D_DESC depthDesc  = {};
    depthDesc.Width                 = width;
    depthDesc.Height                = height;
    depthDesc.MipLevels             = 1;
    depthDesc.ArraySize             = 1;
    depthDesc.Format                = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count      = 1;
    depthDesc.SampleDesc.Quality    = 0;
    depthDesc.Usage                 = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags             = D3D11_BIND_DEPTH_STENCIL;
    SE_HR(m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthTex));

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format                        = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension                 = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice            = 0;
    SE_HR(m_device->CreateDepthStencilView(m_depthTex.Get(), &dsvDesc, &m_dsv));

    // ---- Depth stencil state — Z-test on, write enabled ----
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable              = TRUE;
    dsDesc.DepthWriteMask           = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc                = D3D11_COMPARISON_LESS;
    dsDesc.StencilEnable            = FALSE;
    SE_HR(m_device->CreateDepthStencilState(&dsDesc, &m_depthState));

    // ---- Viewport ----
    D3D11_VIEWPORT vp = {};
    vp.Width          = static_cast<float>(width);
    vp.Height         = static_cast<float>(height);
    vp.MinDepth       = 0.0f;
    vp.MaxDepth       = 1.0f;
    m_context->RSSetViewports(1, &vp);

    SE_LOG_INFO("D3D11 device created — feature level 0x%04X, %ux%u",
                static_cast<unsigned>(achievedLevel), width, height);
    return true;
}

void Renderer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return;

    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();
    m_dsv.Reset();
    m_depthTex.Reset();

    SE_HR(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));

    ComPtr<ID3D11Texture2D> backBuffer;
    SE_HR(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    SE_HR(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv));

    D3D11_TEXTURE2D_DESC depthDesc  = {};
    depthDesc.Width                 = width;
    depthDesc.Height                = height;
    depthDesc.MipLevels             = 1;
    depthDesc.ArraySize             = 1;
    depthDesc.Format                = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count      = 1;
    depthDesc.Usage                 = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags             = D3D11_BIND_DEPTH_STENCIL;
    SE_HR(m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthTex));

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format                        = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension                 = D3D11_DSV_DIMENSION_TEXTURE2D;
    SE_HR(m_device->CreateDepthStencilView(m_depthTex.Get(), &dsvDesc, &m_dsv));

    D3D11_VIEWPORT vp = {};
    vp.Width          = static_cast<float>(width);
    vp.Height         = static_cast<float>(height);
    vp.MaxDepth       = 1.0f;
    m_context->RSSetViewports(1, &vp);

    SE_LOG_INFO("Renderer resized — %ux%u", width, height);
}

void Renderer::Shutdown()
{
    m_depthState.Reset();
    m_dsv.Reset();
    m_depthTex.Reset();
    m_rtv.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
    SE_LOG_INFO("Renderer shut down");
}

void Renderer::BeginFrame(float r, float g, float b, float a)
{
    float color[4] = { r, g, b, a };
    m_context->ClearRenderTargetView(m_rtv.Get(), color);
    m_context->ClearDepthStencilView(m_dsv.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
    m_context->OMSetDepthStencilState(m_depthState.Get(), 0);
}

void Renderer::EndFrame()
{
    m_swapChain->Present(1, 0);
}

} // namespace SE
