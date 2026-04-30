#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "Engine/Renderer/ConstantBuffer.h"

namespace SE {

class LightEnvironment
{
public:
    struct PointLight
    {
        DirectX::XMFLOAT3 position = { 0.0f, 2.0f, 0.0f };
        DirectX::XMFLOAT3 color    = { 1.0f, 1.0f, 1.0f };
        float             radius   = 10.0f;
    };

    // Directional light
    float elevDeg         = 40.0f;
    float azimDeg         = 30.0f;
    float shininess       = 64.0f;
    float lightIntensity  = 3.0f;   // multiplier on lightColor
    float debugLightMode  = 0.0f;   // 0=normal, 1=force lit (shadow=1), 2=show NdotL
    float lightColor[3]   = { 1.0f, 0.95f, 0.85f };
    float ambientColor[3] = { 0.06f, 0.06f, 0.08f };

    // Point lights
    int        numLights = 0;
    PointLight lights[8];

    bool Init(ID3D11Device* device);

    // Fill and bind LightCB (b1) + PointLightCB (b2).
    void BindPS(ID3D11DeviceContext* ctx, DirectX::XMFLOAT3 cameraPos,
                DirectX::XMMATRIX lightViewProj = DirectX::XMMatrixIdentity());

private:
    struct LightCBData
    {
        DirectX::XMFLOAT3 lightDir;    float shininess;
        DirectX::XMFLOAT3 lightColor;  float _pad0;
        DirectX::XMFLOAT3 ambientColor;float _pad1;
        DirectX::XMFLOAT3 cameraPos;   float _pad2;
        DirectX::XMFLOAT4X4 lightViewProj;
    };
    struct PointEntryData { DirectX::XMFLOAT3 pos; float radius; DirectX::XMFLOAT3 color; float _pad; };
    struct PointLightCBData { PointEntryData lights[8]; int num; DirectX::XMFLOAT3 _pad; };

    ConstantBuffer<LightCBData>     m_lightCB;
    ConstantBuffer<PointLightCBData> m_pointLightCB;
};

} // namespace SE
