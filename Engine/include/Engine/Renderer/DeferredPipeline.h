#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include "Engine/Renderer/GBuffer.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/FullscreenQuad.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/RenderTarget.h"
#include "Engine/Renderer/ForwardPipeline.h"

using Microsoft::WRL::ComPtr;

namespace SE {

class AssetManager;
class LightEnvironment;
class ShadowMap;

class DeferredPipeline
{
public:
    bool Init(ID3D11Device* device, AssetManager& assets, ShaderLibrary& shaders);
    void Shutdown();

    // Geometry pass: render opaque meshes into the G-buffer (MRT).
    void BeginGeometryPass(ID3D11DeviceContext* ctx, GBuffer& gb,
                           DirectX::XMMATRIX view, DirectX::XMMATRIX proj);
    void SubmitMesh(const Mesh& mesh, DirectX::XMMATRIX model,
                    const std::vector<ForwardPipeline::SubMat>& subMats);
    void FlushGeometry(ID3D11DeviceContext* ctx);
    void EndGeometryPass(ID3D11DeviceContext* ctx, GBuffer& gb);

    // Set per-object material params for the geometry pass.
    void SetMaterialParams(ID3D11DeviceContext* ctx,
                           DirectX::XMFLOAT3 albedoTint = {1,1,1},
                           float roughnessScale = 1.0f,
                           float metallic = 0.0f);

    // Lighting pass: fullscreen quad reads G-buffer, outputs lit scene to sceneRT.
    // ptShadowSRVs[0..numPtShadowCasters-1] are TextureCube SRVs for the first N point lights.
    void LightingPass(ID3D11DeviceContext* ctx, GBuffer& gb,
                      RenderTarget& sceneRT,
                      LightEnvironment& lights,
                      ShadowMap& shadow,
                      DirectX::XMFLOAT3 cameraPos,
                      DirectX::XMMATRIX viewProj,
                      ID3D11ShaderResourceView* aoSRV = nullptr,
                      ID3D11ShaderResourceView* const* ptShadowSRVs = nullptr,
                      int numPtShadowCasters = 0,
                      float ptShadowBias = 0.015f);

private:
    struct TransformCBData
    {
        DirectX::XMFLOAT4X4 model;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
    };

    struct MaterialCBData
    {
        DirectX::XMFLOAT3 albedoTint; float roughnessScale;
        float metallic; float unlit; float debugShadow; float _pad;
    };

    struct DeferredCBData
    {
        DirectX::XMFLOAT4X4 invViewProj;
        float screenW; float screenH;
        float debugMode; float enableSSAO;
        int   numPointShadowCasters; float pointShadowBias; float _dcpad[2];
    };

    struct RenderItem
    {
        const Mesh*                            mesh;
        DirectX::XMFLOAT4X4                   model;
        const std::vector<ForwardPipeline::SubMat>* subMats;
    };

    const ShaderPermutation*       m_geomPerm = nullptr;
    const ShaderPermutation*       m_lightPerm = nullptr;
    ComPtr<ID3D11InputLayout>      m_geomLayout;
    ComPtr<ID3D11SamplerState>     m_sampler;
    ComPtr<ID3D11SamplerState>     m_pointSampler;
    ComPtr<ID3D11SamplerState>     m_cubeSampler;  // linear-clamp for point shadow cube
    ConstantBuffer<TransformCBData>  m_transformCB;
    ConstantBuffer<MaterialCBData>   m_materialCB;
    ConstantBuffer<DeferredCBData>   m_deferredCB;
    FullscreenQuad                   m_quad;

    DirectX::XMFLOAT4X4 m_view;
    DirectX::XMFLOAT4X4 m_proj;
    std::vector<RenderItem> m_queue;
};

} // namespace SE
