#pragma once
#include <vector>
#include "Engine/Physics/Plane.h"
#include "Engine/Physics/Ray.h"
#include "Engine/Physics/Sphere.h"
#include "Engine/Physics/OBB.h"
#include "Engine/Physics/CharacterController.h"
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

    struct StaticOBB
    {
        OBB   obb;
        float restitution = 0.5f;
        float friction    = 0.4f;
    };

    struct RaycastHit
    {
        enum class Kind { Sphere, Plane, OBB };

        float               t         = 0.0f;
        DirectX::XMFLOAT3   point     = {};
        DirectX::XMFLOAT3   normal    = {};
        TransformComponent* transform = nullptr; // non-null = sphere
        Kind                kind      = Kind::Plane;
    };

    void AddSphere     (TransformComponent* t, RigidBodyComponent* rb, float radius);
    void AddStaticPlane(Plane plane, float restitution = 0.5f, float friction = 0.4f);
    void AddStaticOBB  (OBB obb,    float restitution = 0.5f, float friction = 0.4f);
    void Clear();

    // Returns true and fills hit with the closest intersection along ray.
    bool Raycast(const Ray& ray, RaycastHit& hit) const;

    // Move a character capsule with gravity + collision response (planes and static OBBs).
    // wishVel is the desired horizontal velocity (not yet multiplied by dt).
    void StepCharacter(CharacterController& cc, DirectX::XMFLOAT3 wishVel, float dt);

    // Collision detection + impulse resolution only.
    // Call AFTER Scene::Update() so integration has already run.
    void Step(float dt);

private:
    std::vector<SphereBody>  m_spheres;
    std::vector<StaticPlane> m_planes;
    std::vector<StaticOBB>   m_staticOBBs;

    void ResolveSphereVsPlane (SphereBody& s, const StaticPlane& p);
    void ResolveSphereVsSphere(SphereBody& a, SphereBody& b);
    void ResolveSphereVsOBB   (SphereBody& s, const StaticOBB& o);
};

} // namespace SE
