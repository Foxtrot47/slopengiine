#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/FullscreenQuad.h"

using Microsoft::WRL::ComPtr;

namespace SE {

class IBLEnvironment
{
public:
    bool Init(ID3D11Device* device, ShaderLibrary& shaders);
    void Shutdown();

    // Generate all IBL textures from a loaded equirectangular panorama SRV.
    // Call once after loading the HDR environment map.
    bool Generate(ID3D11Device* device, ID3D11DeviceContext* ctx,
                  ID3D11ShaderResourceView* panoramaSRV);

    ID3D11ShaderResourceView* GetIrradianceSRV()  const { return m_irradianceSRV.Get(); }
    ID3D11ShaderResourceView* GetPrefilteredSRV() const { return m_prefilteredSRV.Get(); }
    ID3D11ShaderResourceView* GetBRDFLutSRV()     const { return m_brdfLutSRV.Get(); }
    bool IsReady() const { return m_ready; }

    static constexpr uint32_t k_envCubeSize     = 512;
    static constexpr uint32_t k_irradianceSize  = 32;
    static constexpr uint32_t k_prefilteredSize = 128;
    static constexpr int      k_prefilteredMips = 5;
    static constexpr uint32_t k_brdfLutSize     = 512;

private:
    struct CBData { int faceIndex; float roughness; float envResolution; float _pad; };

    bool CreateCubemap(ID3D11Device* device, uint32_t size, int mipLevels,
                       ComPtr<ID3D11Texture2D>& outTex,
                       ComPtr<ID3D11ShaderResourceView>& outSRV,
                       bool generateMips = false);

    ComPtr<ID3D11RenderTargetView> CreateFaceRTV(ID3D11Device* device,
                                                  ID3D11Texture2D* tex,
                                                  int face, int mip = 0);

    void RenderFace(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv,
                    uint32_t size, const ShaderPermutation* perm,
                    int face, float roughness = 0.0f);

    // Environment cubemap (from equirect panorama)
    ComPtr<ID3D11Texture2D>          m_envCubeTex;
    ComPtr<ID3D11ShaderResourceView> m_envCubeSRV;

    // Diffuse irradiance cubemap
    ComPtr<ID3D11Texture2D>          m_irradianceTex;
    ComPtr<ID3D11ShaderResourceView> m_irradianceSRV;

    // Pre-filtered specular cubemap (mip chain for roughness)
    ComPtr<ID3D11Texture2D>          m_prefilteredTex;
    ComPtr<ID3D11ShaderResourceView> m_prefilteredSRV;

    // BRDF integration LUT (2D, RG16F)
    ComPtr<ID3D11Texture2D>          m_brdfLutTex;
    ComPtr<ID3D11ShaderResourceView> m_brdfLutSRV;
    ComPtr<ID3D11RenderTargetView>   m_brdfLutRTV;

    const ShaderPermutation* m_equirectPerm   = nullptr;
    const ShaderPermutation* m_irradiancePerm = nullptr;
    const ShaderPermutation* m_prefilteredPerm = nullptr;
    const ShaderPermutation* m_brdfPerm       = nullptr;

    FullscreenQuad           m_quad;
    ConstantBuffer<CBData>   m_cb;
    ComPtr<ID3D11SamplerState> m_linearSampler;

    bool m_ready = false;
};

} // namespace SE
