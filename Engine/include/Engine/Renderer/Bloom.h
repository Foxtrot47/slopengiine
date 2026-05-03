#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/FullscreenQuad.h"
#include "Engine/Renderer/RenderTarget.h"

using Microsoft::WRL::ComPtr;

namespace SE {

// Dual Kawase bloom: threshold → 5-level Kawase downsample → Kawase upsample → additive composite.
// Call Apply() between the final HDR scene draw and tone mapping.
class Bloom
{
public:
    static constexpr int k_numLevels = 5;

    bool Init(ID3D11Device* device, ShaderLibrary& shaders,
              uint32_t width, uint32_t height);
    void Resize(ID3D11Device* device, uint32_t width, uint32_t height);
    void Shutdown();

    // Reads from hdrRT, additively blends bloom result back into hdrRT.
    // hdrRT SRV must not be bound as a PS input on entry.
    void Apply(ID3D11DeviceContext* ctx, RenderTarget& hdrRT);

    bool  enabled   = true;
    float threshold = 0.8f;    // HDR luminance at which bloom begins
    float intensity = 0.04f;   // additive scale of the final bloom contribution
    float scatter   = 0.7f;    // blend weight across upsample levels (0=sharp, 1=spread)

private:
    struct BloomCB
    {
        float texelSizeX, texelSizeY;  // 1 / source dimensions
        float threshold;
        float intensity;
        float scatter;
        float _pad[3];
    };

    void initRTs(ID3D11Device* device, uint32_t w, uint32_t h);
    void shutdownRTs();

    // Bind VS+PS from permutation, override sampler to linear, draw the fullscreen quad.
    void draw(ID3D11DeviceContext* ctx, const ShaderPermutation* perm);

    // Update and bind BloomCB (slot b0).
    void updateCB(ID3D11DeviceContext* ctx, float srcW, float srcH,
                  float thresh, float intens, float scat);

    // Bind rt as RTV + set matching viewport; no clear.
    void bindDest(ID3D11DeviceContext* ctx, RenderTarget& rt);

    RenderTarget m_downChain[k_numLevels];
    RenderTarget m_upChain[k_numLevels - 1];

    const ShaderPermutation* m_thresholdPerm  = nullptr;
    const ShaderPermutation* m_downsamplePerm = nullptr;
    const ShaderPermutation* m_upsamplePerm   = nullptr;
    const ShaderPermutation* m_compositePerm  = nullptr;

    ConstantBuffer<BloomCB>     m_cb;
    FullscreenQuad              m_quad;
    ComPtr<ID3D11BlendState>    m_additiveBlend;
    ComPtr<ID3D11SamplerState>  m_linearSampler;
};

} // namespace SE
