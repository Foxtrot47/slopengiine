#pragma once
#include <vector>
#include <DirectXMath.h>
#include "Engine/Scene/Component.h"

namespace SE {

struct TransformComponent : Component
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 eulerDeg = { 0.0f, 0.0f, 0.0f }; // pitch / yaw / roll in degrees
    float             scale    = 1.0f;

    TransformComponent*              parent   = nullptr;
    std::vector<TransformComponent*> children;

    void SetParent(TransformComponent* newParent);
    void Unparent();

    DirectX::XMMATRIX GetLocalMatrix() const
    {
        using namespace DirectX;
        return XMMatrixScaling(scale, scale, scale)
             * XMMatrixRotationRollPitchYaw(
                   XMConvertToRadians(eulerDeg.x),
                   XMConvertToRadians(eulerDeg.y),
                   XMConvertToRadians(eulerDeg.z))
             * XMMatrixTranslation(position.x, position.y, position.z);
    }

    // Recursively concatenates local matrix with parent chain.
    DirectX::XMMATRIX GetWorldMatrix() const
    {
        return parent ? GetLocalMatrix() * parent->GetWorldMatrix() : GetLocalMatrix();
    }
};

} // namespace SE
