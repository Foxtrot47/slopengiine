#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/Logger.h"

namespace SE {

bool Renderer::Initialize(HWND hwnd, uint32_t width, uint32_t height)
{
    // Swap chain is 1× — we render to a separate MSAA surface and resolve
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

    m_stateCache.Init(m_device.Get());

    // Scene depth state — Z-test LESS, full write, no stencil
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable    = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc      = D3D11_COMPARISON_LESS;
    m_sceneDepthState = m_stateCache.GetDepthStencilState(dsDesc);

    // Verify MSAA support (4x is required by all D3D11 hardware for common formats)
    UINT msaaQuality = 0;
    SE_HR(m_device->CheckMultisampleQualityLevels(
        DXGI_FORMAT_R8G8B8A8_UNORM, k_msaaSamples, &msaaQuality));
    if (msaaQuality == 0)
        SE_LOG_WARN("MSAA %ux reported unsupported — surfaces will still be created", k_msaaSamples);
    else
        SE_LOG_INFO("MSAA %ux available (%u quality levels)", k_msaaSamples, msaaQuality);

    CreateSurfaces(width, height);

    D3D11_VIEWPORT vp = {};
    vp.Width          = static_cast<float>(width);
    vp.Height         = static_cast<float>(height);
    vp.MinDepth       = 0.0f;
    vp.MaxDepth       = 1.0f;
    m_context->RSSetViewports(1, &vp);

    SE_LOG_INFO("D3D11 device created — feature level 0x%04X, %ux%u, MSAA %ux",
                static_cast<unsigned>(achievedLevel), width, height, k_msaaSamples);
    return true;
}

void Renderer::CreateSurfaces(uint32_t width, uint32_t height)
{
    // Back buffer + RTV (resolve target)
    SE_HR(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&m_backBuffer)));
    SE_HR(m_device->CreateRenderTargetView(m_backBuffer.Get(), nullptr, &m_rtv));

    // MSAA colour surface
    D3D11_TEXTURE2D_DESC cd = {};
    cd.Width                = width;
    cd.Height               = height;
    cd.MipLevels            = 1;
    cd.ArraySize            = 1;
    cd.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
    cd.SampleDesc.Count     = k_msaaSamples;
    cd.SampleDesc.Quality   = 0;
    cd.Usage                = D3D11_USAGE_DEFAULT;
    cd.BindFlags            = D3D11_BIND_RENDER_TARGET;
    SE_HR(m_device->CreateTexture2D(&cd, nullptr, &m_msaaColor));
    SE_HR(m_device->CreateRenderTargetView(m_msaaColor.Get(), nullptr, &m_msaaRtv));

    // MSAA depth surface
    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width                = width;
    dd.Height               = height;
    dd.MipLevels            = 1;
    dd.ArraySize            = 1;
    dd.Format               = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count     = k_msaaSamples;
    dd.SampleDesc.Quality   = 0;
    dd.Usage                = D3D11_USAGE_DEFAULT;
    dd.BindFlags            = D3D11_BIND_DEPTH_STENCIL;
    SE_HR(m_device->CreateTexture2D(&dd, nullptr, &m_msaaDepth));

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format                        = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension                 = D3D11_DSV_DIMENSION_TEXTURE2DMS;
    SE_HR(m_device->CreateDepthStencilView(m_msaaDepth.Get(), &dsvDesc, &m_msaaDsv));
}

void Renderer::ReleaseSurfaces()
{
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_msaaDsv.Reset();
    m_msaaDepth.Reset();
    m_msaaRtv.Reset();
    m_msaaColor.Reset();
    m_rtv.Reset();
    m_backBuffer.Reset();
}

void Renderer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return;

    ReleaseSurfaces();
    SE_HR(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));
    CreateSurfaces(width, height);

    D3D11_VIEWPORT vp = {};
    vp.Width          = static_cast<float>(width);
    vp.Height         = static_cast<float>(height);
    vp.MaxDepth       = 1.0f;
    m_context->RSSetViewports(1, &vp);

    SE_LOG_INFO("Renderer resized — %ux%u", width, height);
}

void Renderer::Shutdown()
{
    ReleaseSurfaces();
    m_sceneDepthState = nullptr;
    m_stateCache.Clear();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
    SE_LOG_INFO("Renderer shut down");
}

void Renderer::BeginFrame(float r, float g, float b, float a)
{
    float color[4] = { r, g, b, a };
    m_context->ClearRenderTargetView(m_msaaRtv.Get(), color);
    m_context->ClearDepthStencilView(m_msaaDsv.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    m_context->OMSetRenderTargets(1, m_msaaRtv.GetAddressOf(), m_msaaDsv.Get());
    m_context->OMSetDepthStencilState(m_sceneDepthState, 0);
}

void Renderer::EndFrame()
{
    ResolveToBackBuffer();
    Present();
}

void Renderer::ResolveToBackBuffer()
{
    m_context->ResolveSubresource(
        m_backBuffer.Get(), 0,
        m_msaaColor.Get(),  0,
        DXGI_FORMAT_R8G8B8A8_UNORM);
    BindBackBuffer(m_context.Get());
}

void Renderer::ResolveScene(ID3D11Texture2D* dest, DXGI_FORMAT fmt)
{
    m_context->ResolveSubresource(dest, 0, m_msaaColor.Get(), 0, fmt);
}

void Renderer::BindBackBuffer(ID3D11DeviceContext* ctx)
{
    ID3D11RenderTargetView* rtv = m_rtv.Get();
    ctx->OMSetRenderTargets(1, &rtv, nullptr);

    D3D11_TEXTURE2D_DESC desc;
    m_backBuffer->GetDesc(&desc);
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(desc.Width);
    vp.Height   = static_cast<float>(desc.Height);
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

void Renderer::Present()
{
    m_swapChain->Present(1, 0);
}

} // namespace SE
