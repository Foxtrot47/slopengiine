#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/Mesh.h"

using Microsoft::WRL::ComPtr;

namespace SE {

// Omnidirectional (cube) shadow map using R32_FLOAT colour target.
// Stores linear depth: distance / lightFar per face.
//
// Convention: the first N entries in PointLightCB correspond to shadow maps 0..N-1.
// Ensure shadow-casting lights occupy indices 0..numShadowCasters-1 in LightEnvironment.
class PointShadowMap
{
public:
    bool Init(ID3D11Device* device, ShaderLibrary& shaders, uint32_t resolution = 256);
    void Shutdown();

    // Render one cube face (0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z).
    // Call DrawMesh() between Begin/EndFace for every face before binding the SRV.
    void BeginFace(ID3D11DeviceContext* ctx, int face,
                   DirectX::XMFLOAT3 lightPos, float lightFar);
    void DrawMesh(ID3D11DeviceContext* ctx, const Mesh& mesh, DirectX::XMMATRIX model);
    void EndFace(ID3D11DeviceContext* ctx);

    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }

private:
    struct CBData
    {
        DirectX::XMFLOAT4X4 worldViewProj;
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT3   lightPos;
        float               lightFar;
    };

    uint32_t m_resolution = 0;

    ComPtr<ID3D11Texture2D>          m_cubeTex;
    ComPtr<ID3D11RenderTargetView>   m_rtv[6];
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11Texture2D>          m_depthTex;   // shared D32_FLOAT, reused per face
    ComPtr<ID3D11DepthStencilView>   m_dsv;
    ComPtr<ID3D11InputLayout>        m_layout;
    ComPtr<ID3D11RasterizerState>    m_shadowRS;

    ConstantBuffer<CBData>   m_cb;
    const ShaderPermutation* m_perm = nullptr;

    // State cached from BeginFace, used by DrawMesh
    DirectX::XMMATRIX m_faceViewProj  = {};
    DirectX::XMFLOAT3 m_lightPos      = {};
    float             m_lightFar      = 1.0f;

    // Saved pipeline state, restored by EndFace
    D3D11_VIEWPORT          m_savedVP  = {};
    ID3D11RenderTargetView* m_savedRTV = nullptr;
    ID3D11DepthStencilView* m_savedDSV = nullptr;
    ID3D11RasterizerState*  m_savedRS  = nullptr;

    // Build a left-handed view matrix for the given cube face from lightPos.
    static DirectX::XMMATRIX FaceView(DirectX::XMFLOAT3 lightPos, int face);
};

} // namespace SE
