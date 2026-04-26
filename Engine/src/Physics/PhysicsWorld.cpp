#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Physics/Intersect.h"
#include <cmath>
#include <cfloat>

namespace SE {

using namespace DirectX;

// ---- helpers ---------------------------------------------------------------

static float Dot(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
static XMFLOAT3 Scale(const XMFLOAT3& v, float s)
{
    return { v.x*s, v.y*s, v.z*s };
}
static XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return { a.x+b.x, a.y+b.y, a.z+b.z };
}
static XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return { a.x-b.x, a.y-b.y, a.z-b.z };
}
static float Len(const XMFLOAT3& v)
{
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

// ---- public ----------------------------------------------------------------

void PhysicsWorld::AddSphere(TransformComponent* t, RigidBodyComponent* rb, float radius)
{
    m_spheres.push_back({ t, rb, radius });
}

void PhysicsWorld::AddStaticPlane(Plane plane, float restitution, float friction)
{
    m_planes.push_back({ plane, restitution, friction });
}

void PhysicsWorld::Clear()
{
    m_spheres.clear();
    m_planes.clear();
}

bool PhysicsWorld::Raycast(const Ray& ray, RaycastHit& hit) const
{
    bool  found = false;
    float best  = FLT_MAX;

    for (const auto& sb : m_spheres)
    {
        Sphere s = { sb.transform->position, sb.radius };
        float  t = 0.0f;
        if (!Intersects(ray, s, t) || t >= best) continue;

        best          = t;
        found         = true;
        hit.t         = t;
        hit.point     = ray.PointAt(t);
        hit.transform = sb.transform;

        XMVECTOR n = XMVector3Normalize(
            XMVectorSubtract(XMLoadFloat3(&hit.point), XMLoadFloat3(&sb.transform->position)));
        XMStoreFloat3(&hit.normal, n);
    }

    for (const auto& sp : m_planes)
    {
        float t = 0.0f;
        if (!Intersects(ray, sp.plane, t) || t >= best) continue;

        best          = t;
        found         = true;
        hit.t         = t;
        hit.point     = ray.PointAt(t);
        hit.normal    = sp.plane.normal;
        hit.transform = nullptr;
    }

    return found;
}

void PhysicsWorld::Step(float /*dt*/)
{
    for (auto& s : m_spheres)
    {
        if (s.body->isStatic || !s.body->enabled) continue;

        for (const auto& p : m_planes)
            ResolveSphereVsPlane(s, p);
    }

    for (size_t i = 0; i < m_spheres.size(); ++i)
        for (size_t j = i + 1; j < m_spheres.size(); ++j)
            ResolveSphereVsSphere(m_spheres[i], m_spheres[j]);
}

// ---- private ---------------------------------------------------------------

void PhysicsWorld::ResolveSphereVsPlane(SphereBody& s, const StaticPlane& sp)
{
    const XMFLOAT3& n   = sp.plane.normal;
    float           dist = Dot(s.transform->position, n) - sp.plane.d;

    if (dist >= s.radius) return; // no contact

    // Positional correction — push sphere out of the plane.
    float pen = s.radius - dist;
    s.transform->position = Add(s.transform->position, Scale(n, pen));

    // Only resolve if moving into the plane.
    float vN = Dot(s.body->velocity, n);
    if (vN >= 0.0f) return;

    // Normal impulse with restitution.
    float e = sp.restitution * s.body->restitution;
    s.body->velocity = Add(s.body->velocity, Scale(n, -(1.0f + e) * vN));

    // Friction: damp the tangential component.
    XMFLOAT3 newN  = Scale(n, Dot(s.body->velocity, n));
    XMFLOAT3 vTan  = Sub(s.body->velocity, newN);
    float     mu   = sp.friction * s.body->friction;
    s.body->velocity = Add(newN, Scale(vTan, 1.0f - mu));
}

void PhysicsWorld::ResolveSphereVsSphere(SphereBody& a, SphereBody& b)
{
    if ((a.body->isStatic || !a.body->enabled) &&
        (b.body->isStatic || !b.body->enabled)) return;

    XMFLOAT3 delta = Sub(b.transform->position, a.transform->position);
    float     dist  = Len(delta);
    float     sumR  = a.radius + b.radius;

    if (dist >= sumR || dist < 1e-6f) return;

    XMFLOAT3 n   = Scale(delta, 1.0f / dist);
    float     pen = sumR - dist;

    // Positional correction split by inverse mass.
    float invMA = a.body->isStatic ? 0.0f : 1.0f / a.body->mass;
    float invMB = b.body->isStatic ? 0.0f : 1.0f / b.body->mass;
    float invSum = invMA + invMB;
    if (invSum < 1e-9f) return;

    a.transform->position = Sub(a.transform->position, Scale(n, pen * invMA / invSum));
    b.transform->position = Add(b.transform->position, Scale(n, pen * invMB / invSum));

    // Relative velocity along normal.
    XMFLOAT3 relVel = Sub(b.body->velocity, a.body->velocity);
    float     vN    = Dot(relVel, n);
    if (vN >= 0.0f) return; // separating

    float e = a.body->restitution * b.body->restitution;
    float j = -(1.0f + e) * vN / invSum;

    a.body->velocity = Sub(a.body->velocity, Scale(n, j * invMA));
    b.body->velocity = Add(b.body->velocity, Scale(n, j * invMB));
}

} // namespace SE
