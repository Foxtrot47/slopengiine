#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace SE {

class Renderer
{
public:
    bool Initialize(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    // Clear the back buffer and bind the RTV. Call at the start of each frame.
    void BeginFrame(float r, float g, float b, float a = 1.0f);

    // Present the back buffer. Call at the end of each frame.
    void EndFrame();

    ID3D11Device*        GetDevice()  const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

private:
    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;
    ComPtr<IDXGISwapChain>         m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_rtv;
};

} // namespace SE
