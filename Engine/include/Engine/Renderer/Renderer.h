#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>
#include "Engine/Renderer/RenderStateCache.h"

using Microsoft::WRL::ComPtr;

namespace SE {

class Renderer
{
public:
    bool Initialize(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    // Clears colour + depth on the MSAA surface, binds it. Call at the start of each frame.
    void BeginFrame(float r, float g, float b, float a = 1.0f);

    // Resolves MSAA → back buffer, then presents. Convenience for no-post-process path.
    void EndFrame();

    // Resolve MSAA colour surface to the 1× back buffer and bind it as the current RTV.
    void ResolveToBackBuffer();

    // Resolve MSAA colour surface into a non-MSAA target texture (e.g. RenderTarget).
    void ResolveScene(ID3D11Texture2D* dest,
                      DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM);

    // Bind the swap chain back-buffer RTV (no depth). Used for final fullscreen blit.
    void BindBackBuffer(ID3D11DeviceContext* ctx);

    // Just present the swap chain. Call after post-process + ImGui have drawn to back buffer.
    void Present();

    // Rebuild swap chain buffers and MSAA surfaces after a window resize.
    void Resize(uint32_t width, uint32_t height);

    ID3D11Device*        GetDevice()  const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }
    RenderStateCache&    GetStateCache()    { return m_stateCache; }

private:
    static constexpr UINT k_msaaSamples = 4;

    void CreateSurfaces(uint32_t width, uint32_t height);
    void ReleaseSurfaces();

    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;
    ComPtr<IDXGISwapChain>         m_swapChain;

    // Swap chain back buffer — resolve destination only, never bound as scene RTV
    ComPtr<ID3D11Texture2D>        m_backBuffer;
    ComPtr<ID3D11RenderTargetView> m_rtv;

    // MSAA offscreen surface — scene + UI render here each frame
    ComPtr<ID3D11Texture2D>        m_msaaColor;
    ComPtr<ID3D11RenderTargetView> m_msaaRtv;
    ComPtr<ID3D11Texture2D>        m_msaaDepth;
    ComPtr<ID3D11DepthStencilView> m_msaaDsv;

    RenderStateCache       m_stateCache;
    ID3D11DepthStencilState* m_sceneDepthState = nullptr; // non-owning; owned by m_stateCache
};

} // namespace SE
