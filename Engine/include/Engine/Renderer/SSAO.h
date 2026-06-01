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

// Screen-Space Ambient Occlusion post-process.
// Call Apply() after the forward pass but before bloom/tone mapping.
// Reads depth + normal MRT buffers; outputs AO-darkened HDR scene.
class SSAO
{
public:
    bool Init(ID3D11Device* device, ShaderLibrary& shaders,
              uint32_t width, uint32_t height);
    void Resize(ID3D11Device* device, uint32_t width, uint32_t height);
    void Shutdown();

    // Computes AO from depth+normals and multiplies into hdrRT.
    void Apply(ID3D11DeviceContext* ctx,
               RenderTarget& hdrRT,
               ID3D11ShaderResourceView* depthSRV,
               ID3D11ShaderResourceView* normalSRV,
               DirectX::XMMATRIX proj);

    bool  enabled = true;
    float radius  = 0.5f;   // sampling radius in view-space units
    float bias    = 0.025f; // depth bias to prevent self-occlusion
    float power   = 2.0f;   // AO contrast power

private:
    static constexpr int KERNEL_SIZE = 64;

    struct alignas(16) SSAOCBData
    {
        DirectX::XMFLOAT4X4 invProj;           // 64 bytes
        DirectX::XMFLOAT4X4 proj;              // 64 bytes
        DirectX::XMFLOAT4   samples[KERNEL_SIZE]; // 1024 bytes
        float screenWidth, screenHeight;        // 8 bytes
        float noiseScaleX, noiseScaleY;         // 8 bytes
        float radius;                           // 4 bytes
        float bias;                             // 4 bytes
        float power;                            // 4 bytes
        float _pad;                             // 4 bytes
    };  // Total: 1184 bytes

    struct alignas(16) BlurCBData
    {
        float texelSizeX, texelSizeY;
        float dirX, dirY;
    };

    void GenerateKernel();
    void CreateNoiseTexture(ID3D11Device* device);

    RenderTarget               m_aoRT;         // raw AO result
    RenderTarget               m_blurRT;       // blur intermediate
    const ShaderPermutation*   m_ssaoPerm   = nullptr;
    const ShaderPermutation*   m_blurPerm   = nullptr;
    const ShaderPermutation*   m_applyPerm  = nullptr;
    ConstantBuffer<SSAOCBData> m_cb;
    ConstantBuffer<BlurCBData> m_blurCB;
    FullscreenQuad             m_quad;
    ComPtr<ID3D11ShaderResourceView>  m_noiseSRV;
    ComPtr<ID3D11SamplerState>        m_clampSampler;
    ComPtr<ID3D11SamplerState>        m_wrapSampler;
    ComPtr<ID3D11BlendState>          m_multiplyBlend;
    DirectX::XMFLOAT4                 m_kernel[KERNEL_SIZE];
    uint32_t                   m_width  = 0;
    uint32_t                   m_height = 0;
};

} // namespace SE
