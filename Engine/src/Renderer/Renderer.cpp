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

    // Try with debug layer first; fall back silently if the SDK isn't installed.
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

    // Create render target view from the swap chain back buffer.
    ComPtr<ID3D11Texture2D> backBuffer;
    SE_HR(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    SE_HR(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv));

    // Set a full-window viewport — stays fixed until we add resize support.
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

void Renderer::Shutdown()
{
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
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);
}

void Renderer::EndFrame()
{
    // SyncInterval 1 = vsync on. This is also what locks us to ~60/144 fps
    // instead of the 3M fps we saw in M04.
    m_swapChain->Present(1, 0);
}

} // namespace SE
