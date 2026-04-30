#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/FullscreenQuad.h"
#include "Engine/Renderer/RenderTarget.h"

using Microsoft::WRL::ComPtr;

namespace SE {

class Bloom
{
public:
    static constexpr int k_maxMips = 6;

    bool Init(ID3D11Device* device, ShaderLibrary& shaders,
              uint32_t width, uint32_t height);
    void Shutdown();
    void Resize(ID3D11Device* device, uint32_t width, uint32_t height);

    // Run the full bloom pipeline.  Returns the SRV of the composited result
    // (scene + bloom, still HDR).  Caller feeds this into ToneMap.
    ID3D11ShaderResourceView* Apply(ID3D11DeviceContext* ctx,
                                     ID3D11ShaderResourceView* hdrSceneSRV,
                                     uint32_t sceneW, uint32_t sceneH);

    float threshold = 1.0f;
    float intensity = 0.3f;
    bool  enabled   = true;

private:
    struct CBData { float texelU; float texelV; float threshold; float intensity; };

    void RebuildMips(ID3D11Device* device, uint32_t w, uint32_t h);
    void DrawPass(ID3D11DeviceContext* ctx, const ShaderPermutation* perm);

    FullscreenQuad m_quad;

    const ShaderPermutation* m_extractPerm   = nullptr;
    const ShaderPermutation* m_downPerm      = nullptr;
    const ShaderPermutation* m_upPerm        = nullptr;
    const ShaderPermutation* m_compositePerm = nullptr;

    ConstantBuffer<CBData> m_cb;
    ComPtr<ID3D11SamplerState> m_linearSampler;

    // Mip chain: [0] = half-res, [1] = quarter-res, etc.
    RenderTarget m_mips[k_maxMips];
    int          m_numMips = 0;

    // Final composite target (full scene resolution)
    RenderTarget m_compositeRT;
    uint32_t     m_width = 0, m_height = 0;
};

} // namespace SE
