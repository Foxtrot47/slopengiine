#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <cstdint>
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/FullscreenQuad.h"
#include "Engine/Renderer/RenderTarget.h"

using Microsoft::WRL::ComPtr;

namespace SE {

// Screen-Space Reflections post-process.
// Call Apply() after the forward pass but before bloom/tone mapping.
// Requires: HDR color buffer, readable depth buffer, normal+roughness buffer.
class SSR
{
public:
    bool Init(ID3D11Device* device, ShaderLibrary& shaders,
              uint32_t width, uint32_t height);
    void Resize(ID3D11Device* device, uint32_t width, uint32_t height);
    void Shutdown();

    // Reads from the HDR scene, depth, and normal buffers.
    // Composites reflections back into hdrRT using alpha blending.
    void Apply(ID3D11DeviceContext* ctx,
               RenderTarget& hdrRT,
               ID3D11ShaderResourceView* depthSRV,
               ID3D11ShaderResourceView* normalSRV,
               DirectX::XMMATRIX proj);

    bool  enabled      = true;
    float intensity    = 2.0f;
    float maxDistance  = 4000.0f;
    float thickness    = 5.0f;
    int   maxSteps     = 2000;
    int   binarySteps  = 8;

private:
    struct SSRCBData
    {
        DirectX::XMFLOAT4X4 invProj;
        DirectX::XMFLOAT4X4 proj;
        float screenWidth, screenHeight;
        float maxDistance;
        float thickness;
        float _unused0;
        int   maxSteps;
        int   binarySteps;
        float intensity;
        int   _unused1;
        float _pad[3];
    };

    RenderTarget               m_ssrRT;
    const ShaderPermutation*   m_ssrPerm       = nullptr;
    const ShaderPermutation*   m_compositePerm = nullptr;
    ConstantBuffer<SSRCBData>  m_cb;
    FullscreenQuad             m_quad;
    ComPtr<ID3D11BlendState>   m_alphaBlend;
    ComPtr<ID3D11SamplerState> m_linearSampler;
    uint32_t                   m_width  = 0;
    uint32_t                   m_height = 0;
};

} // namespace SE
