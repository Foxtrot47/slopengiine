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
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/RenderQueue.h"

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

    bool Init(ID3D11Device* device, AssetManager& assets, ShaderLibrary& shaders);

    // Build per-submesh texture handles from asset cache; falls back to default 1x1 textures.
    std::vector<SubMat> LoadMeshMaterials(AssetManager& assets, const Mesh& mesh);

    // Bind shaders + shared pipeline state; cache view/proj for this frame.
    void Begin(ID3D11DeviceContext* ctx, DirectX::XMMATRIX view, DirectX::XMMATRIX proj);

    // Upload and bind MaterialParamsCB (b3).
    void SetMaterialParams(ID3D11DeviceContext* ctx,
                           DirectX::XMFLOAT3 tint, float roughnessScale, float metallic);

    // --- Queued rendering (M42) ---
    // Submit a mesh for sorted draw. Call Flush() after all submits to actually draw.
    void SubmitMesh(const Mesh& mesh, DirectX::XMMATRIX model,
                    const std::vector<SubMat>& mats, bool transparent = false);
    void Flush(ID3D11DeviceContext* ctx);

    // --- Immediate rendering (legacy) ---
    void DrawMesh(ID3D11DeviceContext* ctx, const Mesh& mesh,
                  DirectX::XMMATRIX model, const std::vector<SubMat>& mats);
    void DrawSphere(ID3D11DeviceContext* ctx,
                    DirectX::XMFLOAT3 position, float radius, DirectX::XMFLOAT3 tint);

    // Debug wire draws — unlit flat color, LINELIST topology.
    void DrawWireSphere(ID3D11DeviceContext* ctx,
                        DirectX::XMFLOAT3 position, float radius, DirectX::XMFLOAT3 color);
    void DrawWireAABB(ID3D11DeviceContext* ctx,
                      DirectX::XMFLOAT3 min, DirectX::XMFLOAT3 max, DirectX::XMFLOAT3 color);
    void DrawWireDisc(ID3D11DeviceContext* ctx,
                      DirectX::XMFLOAT3 center, float radius, DirectX::XMFLOAT3 color);
    void DrawWireBox(ID3D11DeviceContext* ctx,
                     DirectX::XMMATRIX world, DirectX::XMFLOAT3 color);
    void DrawLine(ID3D11DeviceContext* ctx,
                  DirectX::XMFLOAT3 from, DirectX::XMFLOAT3 to, DirectX::XMFLOAT3 color);

    uint32_t GetLastDrawCalls() const { return m_lastDrawCalls; }

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
        float metallic; float unlit; float _pad[2];
    };

    // Stored per-submit for Flush() to reference.
    struct QueuedDraw
    {
        const Mesh*              mesh;
        const std::vector<SubMat>* mats;
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_lineBuffer;

    ConstantBuffer<TransformCBData>      m_transformCB;
    ConstantBuffer<MaterialParamsCBData> m_materialCB;
    SamplerState                         m_sampler;
    VertexBuffer                         m_sphereVB;
    IndexBuffer                          m_sphereIB;
    VertexBuffer                         m_wireSphereVB;
    IndexBuffer                          m_wireSphereIB;
    VertexBuffer                         m_wireAABBVB;
    IndexBuffer                          m_wireAABBIB;

    AssetHandle<Texture2D> m_defaultWhite;
    AssetHandle<Texture2D> m_defaultNormal;

    DirectX::XMMATRIX m_view = {};
    DirectX::XMMATRIX m_proj = {};

    RenderQueue              m_queue;
    std::vector<QueuedDraw>  m_queuedDraws;
    uint32_t                 m_lastDrawCalls = 0;
};

} // namespace SE
