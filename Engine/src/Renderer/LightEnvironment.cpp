#include "Engine/Renderer/LightEnvironment.h"
#include <cmath>

namespace SE {

bool LightEnvironment::Init(ID3D11Device* device)
{
    if (!m_lightCB.Create(device))      return false;
    if (!m_pointLightCB.Create(device)) return false;
    return true;
}

void LightEnvironment::BindPS(ID3D11DeviceContext* ctx, DirectX::XMFLOAT3 cameraPos,
                               DirectX::XMMATRIX lightViewProj)
{
    using namespace DirectX;

    float er = XMConvertToRadians(elevDeg);
    float ar = XMConvertToRadians(azimDeg);

    LightCBData lc;
    lc.lightDir      = { cosf(er) * sinf(ar), sinf(er), cosf(er) * cosf(ar) };
    lc.iblIntensity  = iblIntensity;
    lc.lightColor    = { lightColor[0] * lightIntensity,
                         lightColor[1] * lightIntensity,
                         lightColor[2] * lightIntensity };
    lc._pad0         = 0.0f;
    lc.cameraPos     = cameraPos;
    lc.debugLightMode = debugLightMode;
    XMStoreFloat4x4(&lc.lightViewProj, lightViewProj);
    m_lightCB.Update(ctx, lc);
    m_lightCB.BindPS(ctx, 1);

    PointLightCBData pl = {};
    pl.num = numLights;
    for (int i = 0; i < numLights; ++i)
    {
        pl.lights[i].pos    = lights[i].position;
        pl.lights[i].radius = lights[i].radius;
        pl.lights[i].color  = lights[i].color;
        pl.lights[i]._pad   = 0.0f;
    }
    m_pointLightCB.Update(ctx, pl);
    m_pointLightCB.BindPS(ctx, 2);
}

} // namespace SE
