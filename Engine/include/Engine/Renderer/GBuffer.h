#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace SE {

class GBuffer
{
public:
    bool Init(ID3D11Device* device, uint32_t width, uint32_t height);
    void Shutdown();

    // Bind all 3 colour RTVs + depth DSV as MRT. Clears all targets.
    void Bind(ID3D11DeviceContext* ctx);

    // Restore previously saved RTV/DSV/viewport.
    void Unbind(ID3D11DeviceContext* ctx);

    // Bind G-buffer textures as SRVs for the lighting pass.
    //   t0 = albedo, t1 = normal, t2 = material, t3 = depth
    void BindForLighting(ID3D11DeviceContext* ctx) const;
    void UnbindLighting(ID3D11DeviceContext* ctx) const;

    // Bind depth as DSV + scene RTV (no G-buffer colour) for sky/forward compositing.
    void BindDepthForComposite(ID3D11DeviceContext* ctx,
                                ID3D11RenderTargetView* sceneRTV) const;

    ID3D11DepthStencilView*   GetDSV() const { return m_dsv.Get(); }
    ID3D11ShaderResourceView* GetDepthSRV() const { return m_depthSRV.Get(); }
    uint32_t GetWidth()  const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
    static constexpr int k_numColorTargets = 3;

    ComPtr<ID3D11Texture2D>          m_colorTex[k_numColorTargets];
    ComPtr<ID3D11RenderTargetView>   m_colorRTV[k_numColorTargets];
    ComPtr<ID3D11ShaderResourceView> m_colorSRV[k_numColorTargets];

    ComPtr<ID3D11Texture2D>          m_depthTex;
    ComPtr<ID3D11DepthStencilView>   m_dsv;
    ComPtr<ID3D11ShaderResourceView> m_depthSRV;

    uint32_t m_width  = 0;
    uint32_t m_height = 0;

    // Saved state
    ID3D11RenderTargetView* m_savedRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* m_savedDSV = nullptr;
    D3D11_VIEWPORT          m_savedVP  = {};
    UINT                    m_savedNumVPs = 0;
};

} // namespace SE
