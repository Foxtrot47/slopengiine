#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/ConstantBuffer.h"

namespace SE {

class Material
{
public:
    bool Create(ID3D11Device* device);
    bool LoadAlbedo(ID3D11Device* device, const wchar_t* path);
    bool LoadRoughness(ID3D11Device* device, const wchar_t* path);
    bool LoadNormal(ID3D11Device* device, const wchar_t* path);

    // Binds albedo(t0), roughness(t1), normal(t2), MaterialCB(b3)
    void Bind(ID3D11DeviceContext* ctx);

    float albedoTint[3]  = { 1.0f, 1.0f, 1.0f };
    float roughnessScale = 1.0f;
    float metallic       = 0.0f;

private:
    struct MaterialData {
        DirectX::XMFLOAT3 albedoTint;    float roughnessScale;
        float             metallic;       DirectX::XMFLOAT3 _pad;
    };

    Texture2D                    m_albedo;
    Texture2D                    m_roughness;
    Texture2D                    m_normal;
    ConstantBuffer<MaterialData> m_cb;
};

} // namespace SE
