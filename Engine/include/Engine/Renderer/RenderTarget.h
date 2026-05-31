#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace SE {

class RenderTarget
{
public:
    bool Init(ID3D11Device* device, uint32_t width, uint32_t height,
              DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
              bool withDepth = false, bool depthReadable = false);
    void Shutdown();

    // Save current RTV/DSV/viewport, bind this target, optionally clear.
    void Begin(ID3D11DeviceContext* ctx, const float clearColor[4] = nullptr);

    // Restore the previously saved RTV/DSV/viewport.
    void End(ID3D11DeviceContext* ctx);

    void BindPS(ID3D11DeviceContext* ctx, UINT slot) const;
    void UnbindPS(ID3D11DeviceContext* ctx, UINT slot) const;

    // Bind the depth SRV to a PS slot (only valid if depthReadable=true).
    void BindDepthPS(ID3D11DeviceContext* ctx, UINT slot) const;
    void UnbindDepthPS(ID3D11DeviceContext* ctx, UINT slot) const;

    ID3D11ShaderResourceView* GetSRV()      const { return m_srv.Get(); }
    ID3D11ShaderResourceView* GetDepthSRV() const { return m_depthSrv.Get(); }
    ID3D11RenderTargetView*   GetRTV()      const { return m_rtv.Get(); }
    ID3D11DepthStencilView*   GetDSV()      const { return m_dsv.Get(); }
    ID3D11Texture2D*          GetTexture()  const { return m_tex.Get(); }
    uint32_t GetWidth()  const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
    ComPtr<ID3D11Texture2D>          m_tex;
    ComPtr<ID3D11RenderTargetView>   m_rtv;
    ComPtr<ID3D11ShaderResourceView> m_srv;

    ComPtr<ID3D11Texture2D>          m_depthTex;
    ComPtr<ID3D11DepthStencilView>   m_dsv;
    ComPtr<ID3D11ShaderResourceView> m_depthSrv;  // only if depthReadable

    uint32_t m_width  = 0;
    uint32_t m_height = 0;

    // State saved by Begin(), restored by End()
    ID3D11RenderTargetView*  m_savedRtv = nullptr;
    ID3D11DepthStencilView*  m_savedDsv = nullptr;
    D3D11_VIEWPORT           m_savedVP  = {};
};

} // namespace SE
