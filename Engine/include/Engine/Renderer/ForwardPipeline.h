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
#include "Engine/Renderer/Frustum.h"

namespace SE {

class ForwardPipeline
{
public:
    struct SubMat
    {
        AssetHandle<Texture2D> albedo;
        AssetHandle<Texture2D> normal;
        AssetHandle<Texture2D> roughness;
        AssetHandle<Texture2D> metallic;  // t7; nullptr → default black (0 = dielectric)
    };

    bool Init(ID3D11Device* device, AssetManager& assets, ShaderLibrary& shaders);

    // Build per-submesh texture handles from asset cache; falls back to default 1x1 textures.
    std::vector<SubMat> LoadMeshMaterials(AssetManager& assets, const Mesh& mesh);

    // Bind shaders + shared pipeline state; cache view/proj for this frame.
    // Also binds a default ForwardShadowCB (b4) with zero point shadow casters.
    void Begin(ID3D11DeviceContext* ctx, DirectX::XMMATRIX view, DirectX::XMMATRIX proj);

    // Bind equirectangular HDR panorama for IBL (t4). Pass nullptr to unbind.
    void BindEnvironment(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* panoramaSRV);

    // Bind point shadow cube maps (t5-t6) + ForwardShadowCB (b4). Pass nullptr SRVs if unused.
    void BindPointShadows(ID3D11DeviceContext* ctx,
                          ID3D11ShaderResourceView* psrv0,
                          ID3D11ShaderResourceView* psrv1,
                          int numCasters, float bias);

    // Upload and bind MaterialParamsCB (b3).
    void SetMaterialParams(ID3D11DeviceContext* ctx,
                           DirectX::XMFLOAT3 tint, float roughnessScale, float metallic,
                           float debugShadow = 0.0f);

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

    // Draw a sphere with explicit PBR textures and a metallic override scalar.
    // Uses the SubMat's albedo/normal/roughness textures. Set metallic per-sphere via SetMaterialParams.
    void DrawPBRSphere(ID3D11DeviceContext* ctx,
                       DirectX::XMFLOAT3 position, float radius,
                       const SubMat& mat, float metallic, float roughnessScale = 1.0f,
                       DirectX::XMFLOAT3 tint = { 1.f, 1.f, 1.f });

    // Draw a horizontal unit plane scaled to the given half-extents, with PBR textures.
    void DrawPBRPlane(ID3D11DeviceContext* ctx,
                      DirectX::XMFLOAT3 center, float halfSizeX, float halfSizeZ,
                      const SubMat& mat, float metallic, float roughnessScale = 1.0f,
                      DirectX::XMFLOAT3 tint = { 1.f, 1.f, 1.f });

    uint32_t GetLastDrawCalls() const { return m_lastDrawCalls; }
    uint32_t GetLastCulledCount() const { return m_lastCulled; }

private:
    struct ForwardShadowCBData
    {
        int   numPointShadowCasters;
        float pointShadowBias;
        float _pad[2];
    };

    struct TransformCBData
    {
        DirectX::XMFLOAT4X4 model;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
    };
    struct MaterialParamsCBData
    {
        DirectX::XMFLOAT3 albedoTint; float roughnessScale;
        float metallic; float unlit; float debugShadow; float _pad2;
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

    ConstantBuffer<TransformCBData>           m_transformCB;
    ConstantBuffer<MaterialParamsCBData>      m_materialCB;
    ConstantBuffer<ForwardShadowCBData>       m_shadowCB;
    SamplerState                              m_sampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_cubeSampler;
    VertexBuffer                         m_sphereVB;
    IndexBuffer                          m_sphereIB;
    VertexBuffer                         m_wireSphereVB;
    IndexBuffer                          m_wireSphereIB;
    VertexBuffer                         m_wireAABBVB;
    IndexBuffer                          m_wireAABBIB;
    VertexBuffer                         m_planeVB;
    IndexBuffer                          m_planeIB;

    AssetHandle<Texture2D> m_defaultWhite;
    AssetHandle<Texture2D> m_defaultBlack;
    AssetHandle<Texture2D> m_defaultNormal;

    DirectX::XMMATRIX m_view = {};
    DirectX::XMMATRIX m_proj = {};
    Frustum                  m_frustum;

    RenderQueue              m_queue;
    std::vector<QueuedDraw>  m_queuedDraws;
    uint32_t                 m_lastDrawCalls = 0;
    uint32_t                 m_lastCulled    = 0;
};

} // namespace SE
