#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include "Engine/Renderer/RenderTarget.h"
#include "Engine/Renderer/FullscreenQuad.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/ShaderLibrary.h"

using Microsoft::WRL::ComPtr;

namespace SE {

class GBuffer;

class SSAO
{
public:
    bool Init(ID3D11Device* device, ShaderLibrary& shaders, uint32_t w, uint32_t h);
    void Shutdown();
    void Resize(ID3D11Device* device, uint32_t w, uint32_t h);

    // Run SSAO generation + bilateral blur. Result in GetAOSRV().
    void Render(ID3D11DeviceContext* ctx, GBuffer& gb,
                DirectX::XMMATRIX view, DirectX::XMMATRIX proj);

    ID3D11ShaderResourceView* GetAOSRV() const { return m_aoRT.GetSRV(); }

    // Tweakable parameters
    float radius    = 0.5f;
    float bias      = 0.025f;
    float intensity = 1.5f;
    int   kernelSize = 32;
    bool  enabled    = true;

private:
    static constexpr int k_maxKernelSize = 64;
    static constexpr int k_noiseSize     = 4;

    struct SSAOParamsCB
    {
        DirectX::XMFLOAT4X4 projection;
        DirectX::XMFLOAT4X4 invProjection;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4   samples[k_maxKernelSize];
        DirectX::XMFLOAT2   screenSize;
        DirectX::XMFLOAT2   noiseScale;
        float radius;
        float bias;
        float intensity;
        int   kernelSize;
    };

    struct BlurParamsCB
    {
        DirectX::XMFLOAT2 blurDirection;
        DirectX::XMFLOAT2 _pad;
    };

    RenderTarget m_aoRT;      // raw AO (R8_UNORM)
    RenderTarget m_blurRT;    // temp blur target (R8_UNORM)

    FullscreenQuad m_quad;

    ComPtr<ID3D11Texture2D>          m_noiseTex;
    ComPtr<ID3D11ShaderResourceView> m_noiseSRV;
    ComPtr<ID3D11SamplerState>       m_pointClampSampler;
    ComPtr<ID3D11SamplerState>       m_pointWrapSampler;

    ConstantBuffer<SSAOParamsCB> m_paramsCB;
    ConstantBuffer<BlurParamsCB> m_blurCB;

    const ShaderPermutation* m_ssaoPerm = nullptr;
    const ShaderPermutation* m_blurPerm = nullptr;

    std::vector<DirectX::XMFLOAT4> m_kernel;

    void GenerateKernel();
    bool CreateNoiseTexture(ID3D11Device* device);
};

} // namespace SE
