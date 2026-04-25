#pragma once
#include <vector>
#include "Engine/Physics/Plane.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Physics/RigidBodyComponent.h"

namespace SE {

class PhysicsWorld
{
public:
    struct SphereBody
    {
        TransformComponent* transform;
        RigidBodyComponent* body;
        float               radius;
    };

    struct StaticPlane
    {
        Plane plane;
        float restitution = 0.5f;
        float friction    = 0.4f;
    };

    void AddSphere    (TransformComponent* t, RigidBodyComponent* rb, float radius);
    void AddStaticPlane(Plane plane, float restitution = 0.5f, float friction = 0.4f);
    void Clear();

    // Collision detection + impulse resolution only.
    // Call AFTER Scene::Update() so integration has already run.
    void Step(float dt);

private:
    std::vector<SphereBody>  m_spheres;
    std::vector<StaticPlane> m_planes;

    void ResolveSphereVsPlane (SphereBody& s, const StaticPlane& p);
    void ResolveSphereVsSphere(SphereBody& a, SphereBody& b);
};

} // namespace SE
