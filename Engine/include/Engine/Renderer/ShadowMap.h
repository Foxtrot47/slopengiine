#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/Mesh.h"

namespace SE {

class ShadowMap
{
public:
    bool Init(ID3D11Device* device, ShaderLibrary& shaders, uint32_t resolution = 2048);

    // Compute light view-projection matrix from directional light direction + scene bounds.
    void UpdateLightMatrix(DirectX::XMFLOAT3 lightDir,
                           DirectX::XMFLOAT3 sceneCentre, float sceneRadius);

    // Bind shadow depth texture as render target; clear depth.
    void BeginShadowPass(ID3D11DeviceContext* ctx);

    // Draw a mesh into the shadow map.
    void DrawMesh(ID3D11DeviceContext* ctx, const Mesh& mesh, DirectX::XMMATRIX model);

    // Unbind shadow RT; restore previous viewport / render targets.
    void EndShadowPass(ID3D11DeviceContext* ctx);

    // Bind shadow map SRV + sampler for the lit pass (t3, s1).
    void BindForLitPass(ID3D11DeviceContext* ctx);

    // Unbind shadow SRV to avoid D3D warnings when rendering to it next frame.
    void Unbind(ID3D11DeviceContext* ctx);

    DirectX::XMMATRIX GetLightViewProj() const { return m_lightViewProj; }
    uint32_t GetResolution() const { return m_resolution; }

private:
    struct ShadowCBData
    {
        DirectX::XMFLOAT4X4 model;
        DirectX::XMFLOAT4X4 viewProj;
    };

    uint32_t m_resolution = 2048;
    DirectX::XMMATRIX m_lightViewProj = {};

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_depthTex;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_dsv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_srv;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>        m_shadowSampler;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>         m_layout;

    const ShaderPermutation* m_perm = nullptr;
    ConstantBuffer<ShadowCBData> m_cb;

    // Saved state to restore after shadow pass.
    D3D11_VIEWPORT         m_savedVP     = {};
    ID3D11RenderTargetView* m_savedRTV   = nullptr;
    ID3D11DepthStencilView* m_savedDSV   = nullptr;
};

} // namespace SE
