#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include "Engine/Assets/AssetManager.h"
#include "Engine/Renderer/VertexBuffer.h"
#include "Engine/Renderer/IndexBuffer.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/SamplerState.h"

namespace SE {

class ForwardPipeline
{
public:
    struct SubMat
    {
        AssetHandle<Texture2D> albedo;
        AssetHandle<Texture2D> normal;
        AssetHandle<Texture2D> roughness;
    };

    bool Init(ID3D11Device* device, AssetManager& assets);

    // Build per-submesh texture handles from asset cache; falls back to default 1x1 textures.
    std::vector<SubMat> LoadMeshMaterials(AssetManager& assets, const Mesh& mesh);

    // Bind shaders + shared pipeline state; cache view/proj for this frame.
    void Begin(ID3D11DeviceContext* ctx, DirectX::XMMATRIX view, DirectX::XMMATRIX proj);

    // Upload and bind MaterialParamsCB (b3).
    void SetMaterialParams(ID3D11DeviceContext* ctx,
                           DirectX::XMFLOAT3 tint, float roughnessScale, float metallic);

    // Draw an indexed mesh at a given world transform with per-submesh textures.
    void DrawMesh(ID3D11DeviceContext* ctx, const Mesh& mesh,
                  DirectX::XMMATRIX model, const std::vector<SubMat>& mats);

    // Draw the debug sphere at position scaled by radius with a flat color tint.
    void DrawSphere(ID3D11DeviceContext* ctx,
                    DirectX::XMFLOAT3 position, float radius, DirectX::XMFLOAT3 tint);

private:
    struct TransformCBData
    {
        DirectX::XMFLOAT4X4 model;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
    };
    struct MaterialParamsCBData
    {
        DirectX::XMFLOAT3 albedoTint; float roughnessScale;
        float metallic; float _pad[3];
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_layout;

    ConstantBuffer<TransformCBData>      m_transformCB;
    ConstantBuffer<MaterialParamsCBData> m_materialCB;
    SamplerState                         m_sampler;
    VertexBuffer                         m_sphereVB;
    IndexBuffer                          m_sphereIB;

    AssetHandle<Texture2D> m_defaultWhite;
    AssetHandle<Texture2D> m_defaultNormal;

    DirectX::XMMATRIX m_view = {};
    DirectX::XMMATRIX m_proj = {};
};

} // namespace SE
