#include "Engine/Physics/RigidBodyComponent.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/TransformComponent.h"

namespace SE {

static constexpr float kGravity = -9.81f;

void RigidBodyComponent::AddForce(DirectX::XMFLOAT3 f)
{
    force.x += f.x;
    force.y += f.y;
    force.z += f.z;
}

void RigidBodyComponent::AddImpulse(DirectX::XMFLOAT3 impulse)
{
    velocity.x += impulse.x;
    velocity.y += impulse.y;
    velocity.z += impulse.z;
}

void RigidBodyComponent::Update(float dt)
{
    if (isStatic || !enabled) return;

    auto* t = GetOwner()->GetComponent<TransformComponent>();
    if (!t) return;

    if (useGravity)
        force.y += kGravity * mass;

    // Semi-implicit Euler: integrate velocity, then position.
    float invMass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
    velocity.x += force.x * invMass * dt;
    velocity.y += force.y * invMass * dt;
    velocity.z += force.z * invMass * dt;

    t->position.x += velocity.x * dt;
    t->position.y += velocity.y * dt;
    t->position.z += velocity.z * dt;

    force = { 0.0f, 0.0f, 0.0f };
}

} // namespace SE
