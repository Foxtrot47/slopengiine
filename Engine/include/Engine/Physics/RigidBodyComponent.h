#pragma once
#include <DirectXMath.h>
#include "Engine/Scene/Component.h"

namespace SE {

struct RigidBodyComponent : Component
{
    float mass       = 1.0f;
    bool  isStatic   = false;
    bool  useGravity = true;

    float restitution = 0.6f; // bounciness [0=dead, 1=perfectly elastic]
    float friction    = 0.4f; // surface friction coefficient

    DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 force    = { 0.0f, 0.0f, 0.0f }; // accumulated; cleared after each step

    // Sustained force applied over time (F = ma).
    void AddForce(DirectX::XMFLOAT3 f);
    // Instantaneous velocity change, independent of mass.
    void AddImpulse(DirectX::XMFLOAT3 impulse);

    void Update(float dt) override;
};

} // namespace SE
