#pragma once
#include <DirectXMath.h>
#include "Engine/Scene/Component.h"

namespace SE {

struct TransformComponent : Component
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 eulerDeg = { 0.0f, 0.0f, 0.0f }; // pitch / yaw / roll in degrees
    float             scale    = 1.0f;

    DirectX::XMMATRIX GetMatrix() const
    {
        using namespace DirectX;
        return XMMatrixScaling(scale, scale, scale)
             * XMMatrixRotationRollPitchYaw(
                   XMConvertToRadians(eulerDeg.x),
                   XMConvertToRadians(eulerDeg.y),
                   XMConvertToRadians(eulerDeg.z))
             * XMMatrixTranslation(position.x, position.y, position.z);
    }
};

} // namespace SE
