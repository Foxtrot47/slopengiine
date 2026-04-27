#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "Engine/Renderer/VertexBuffer.h"
#include "Engine/Renderer/IndexBuffer.h"
#include "Engine/Renderer/ConstantBuffer.h"

namespace SE {

class SkyboxRenderer
{
public:
    bool Init(ID3D11Device* device);
    bool LoadPanorama(ID3D11Device* device, const wchar_t* path);

    // Strip translation from view, draw unit cube at far plane.
    void Draw(ID3D11DeviceContext* ctx, DirectX::XMMATRIX view, DirectX::XMMATRIX proj);

private:
    struct SkyboxCB { DirectX::XMFLOAT4X4 viewProjNoTrans; };

    Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_layout;
    ConstantBuffer<SkyboxCB>                         m_cb;
    VertexBuffer                                     m_cubeVB;
    IndexBuffer                                      m_cubeIB;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_panoramaTex;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_panoramaSRV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_depthState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>    m_rsState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_sampler;
};

} // namespace SE
