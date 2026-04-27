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

void PhysicsWorld::AddStaticOBB(OBB obb, float restitution, float friction)
{
    m_staticOBBs.push_back({ obb, restitution, friction });
}

void PhysicsWorld::Clear()
{
    m_spheres.clear();
    m_planes.clear();
    m_staticOBBs.clear();
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
        hit.kind      = RaycastHit::Kind::Sphere;

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
        hit.kind      = RaycastHit::Kind::Plane;
    }

    for (const auto& so : m_staticOBBs)
    {
        float     t  = 0.0f;
        XMFLOAT3  n  = {};
        if (!Intersects(ray, so.obb, t, n) || t >= best) continue;

        best          = t;
        found         = true;
        hit.t         = t;
        hit.point     = ray.PointAt(t);
        hit.normal    = n;
        hit.transform = nullptr;
        hit.kind      = RaycastHit::Kind::OBB;
    }

    return found;
}

void PhysicsWorld::StepCharacter(CharacterController& cc, XMFLOAT3 wishVel, float dt)
{
    const float cosSlope = cosf(XMConvertToRadians(cc.slopeLimit));
    // Contacts within this distance of the sphere surface register as touching
    // even with zero penetration. Fixes isGrounded at rest (dist == radius exactly).
    const float skin = 0.02f;

    // ---- Gravity ----
    bool wasGrounded = cc.isGrounded;
    if (!wasGrounded && cc.gravityEnabled)
        cc.velY -= cc.gravAccel * dt;

    cc.isGrounded = false;
    XMFLOAT3 accNormal = { 0.0f, 0.0f, 0.0f };

    // Register any surface contact; accumulate normals for Jump() direction.
    // Push (positional correction) is separate and only happens on actual penetration.
    auto addContact = [&](XMFLOAT3 n) {
        accNormal.x += n.x;
        accNormal.y += n.y;
        accNormal.z += n.z;
        cc.isGrounded = true;
    };

    // ---- Vertical pass (sub-stepped to prevent tunneling) ----
    // Each sub-step moves at most 0.8*radius so the sphere never skips
    // past a surface. At normal speeds this is 1 iteration; fast falls
    // add up to 8. Beyond that (extreme edge case) there can still be
    // tunneling, but it requires velocities unreachable in normal play.
    auto bottomCenter = [&]() -> XMFLOAT3 {
        return { cc.position.x, cc.position.y + cc.radius, cc.position.z };
    };

    const float safeStep = cc.radius * 0.8f;
    int   substeps = static_cast<int>(ceilf(fabsf(cc.velY * dt) / safeStep));
    if (substeps < 1) substeps = 1;
    if (substeps > 8) substeps = 8;
    float subDt = dt / substeps;

    for (int s = 0; s < substeps; ++s)
    {
        cc.position.y += cc.velY * subDt;

        for (const auto& sp : m_planes)
        {
            XMFLOAT3 c    = bottomCenter();
            float    dist = Dot(c, sp.plane.normal) - sp.plane.d;
            if (dist >= cc.radius + skin) continue;
            const XMFLOAT3& n = sp.plane.normal;
            addContact(n);
            if (n.y > cosSlope && cc.velY < 0.0f) cc.velY = 0.0f;
            if (dist < cc.radius) {
                float pen = cc.radius - dist;
                cc.position.x += n.x * pen;
                cc.position.y += n.y * pen;
                cc.position.z += n.z * pen;
            }
        }

        for (const auto& so : m_staticOBBs)
        {
            XMFLOAT3 c = bottomCenter();
            XMFLOAT3 d = Sub(c, so.obb.center);
            XMFLOAT3 closest = so.obb.center;
            for (int i = 0; i < 3; ++i)
            {
                float proj = Dot(d, so.obb.axes[i]);
                float he   = (&so.obb.halfExtents.x)[i];
                float p    = proj < -he ? -he : (proj > he ? he : proj);
                closest.x += p * so.obb.axes[i].x;
                closest.y += p * so.obb.axes[i].y;
                closest.z += p * so.obb.axes[i].z;
            }
            XMFLOAT3 delta = Sub(c, closest);
            float     dist  = Len(delta);
            if (dist >= cc.radius + skin || dist < 1e-6f) continue;
            XMFLOAT3 n = Scale(delta, 1.0f / dist);
            addContact(n);
            if (n.y > cosSlope && cc.velY < 0.0f) cc.velY = 0.0f;
            if (dist < cc.radius) {
                float pen = cc.radius - dist;
                cc.position.x += n.x * pen;
                cc.position.y += n.y * pen;
                cc.position.z += n.z * pen;
            }
        }

        if (cc.velY == 0.0f) break; // grounded mid-step, no need to continue
    }

    // ---- Horizontal pass ----
    const float decay = fmaxf(0.0f, 1.0f - 6.0f * dt);
    cc.physVelX *= decay;
    cc.physVelZ *= decay;
    cc.position.x += (wishVel.x + cc.physVelX) * dt;
    cc.position.z += (wishVel.z + cc.physVelZ) * dt;

    for (const auto& sp : m_planes)
    {
        XMFLOAT3 c    = bottomCenter();
        float    dist = Dot(c, sp.plane.normal) - sp.plane.d;
        if (dist >= cc.radius + skin || sp.plane.normal.y > cosSlope) continue;
        addContact(sp.plane.normal);
        if (dist < cc.radius) {
            float pen = cc.radius - dist;
            cc.position.x += sp.plane.normal.x * pen;
            cc.position.z += sp.plane.normal.z * pen;
        }
    }

    for (const auto& so : m_staticOBBs)
    {
        XMFLOAT3 c = bottomCenter();
        XMFLOAT3 d = Sub(c, so.obb.center);
        XMFLOAT3 closest = so.obb.center;
        for (int i = 0; i < 3; ++i)
        {
            float proj = Dot(d, so.obb.axes[i]);
            float he   = (&so.obb.halfExtents.x)[i];
            float p    = proj < -he ? -he : (proj > he ? he : proj);
            closest.x += p * so.obb.axes[i].x;
            closest.y += p * so.obb.axes[i].y;
            closest.z += p * so.obb.axes[i].z;
        }
        XMFLOAT3 delta = Sub(c, closest);
        float     dist  = Len(delta);
        if (dist >= cc.radius + skin || dist < 1e-6f) continue;
        XMFLOAT3 n = Scale(delta, 1.0f / dist);
        if (n.y > cosSlope) continue; // floor — handled in vertical pass

        float obbMaxY = so.obb.center.y
            + fabsf(so.obb.axes[0].y) * so.obb.halfExtents.x
            + fabsf(so.obb.axes[1].y) * so.obb.halfExtents.y
            + fabsf(so.obb.axes[2].y) * so.obb.halfExtents.z;

        if (obbMaxY > cc.position.y && obbMaxY <= cc.position.y + cc.stepHeight)
        {
            if (dist < cc.radius) {
                cc.position.y = obbMaxY;
                if (cc.velY < 0.0f) cc.velY = 0.0f;
            }
            addContact({ 0.0f, 1.0f, 0.0f });
        }
        else
        {
            if (dist < cc.radius) {
                float pen = cc.radius - dist;
                cc.position.x += n.x * pen;
                cc.position.z += n.z * pen;
            }
            addContact(n);
        }
    }

    // ---- Finalize contact normal ----
    float len = Len(accNormal);
    if (len > 1e-6f)
        cc.contactNormal = Scale(accNormal, 1.0f / len);
}

void PhysicsWorld::Step(float /*dt*/)
{
    for (auto& s : m_spheres)
    {
        if (s.body->isStatic || !s.body->enabled) continue;

        for (const auto& p : m_planes)  ResolveSphereVsPlane(s, p);
        for (const auto& o : m_staticOBBs) ResolveSphereVsOBB(s, o);
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

void PhysicsWorld::ResolveSphereVsOBB(SphereBody& s, const StaticOBB& so)
{
    const OBB&    obb = so.obb;
    const XMFLOAT3& C = s.transform->position;

    // Closest point on OBB to sphere centre.
    XMFLOAT3 d       = Sub(C, obb.center);
    XMFLOAT3 closest = obb.center;
    for (int i = 0; i < 3; ++i)
    {
        float proj = Dot(d, obb.axes[i]);
        float he   = (&obb.halfExtents.x)[i];
        float p    = proj < -he ? -he : (proj > he ? he : proj);
        closest.x += p * obb.axes[i].x;
        closest.y += p * obb.axes[i].y;
        closest.z += p * obb.axes[i].z;
    }

    XMFLOAT3 delta = Sub(C, closest);
    float     dist  = Len(delta);

    if (dist >= s.radius) return;

    XMFLOAT3 n = (dist < 1e-6f)
        ? XMFLOAT3{ 0.0f, 1.0f, 0.0f }
        : Scale(delta, 1.0f / dist);

    float pen = s.radius - dist;
    s.transform->position = Add(s.transform->position, Scale(n, pen));

    float vN = Dot(s.body->velocity, n);
    if (vN >= 0.0f) return;

    float e = so.restitution * s.body->restitution;
    s.body->velocity = Add(s.body->velocity, Scale(n, -(1.0f + e) * vN));

    XMFLOAT3 newN = Scale(n, Dot(s.body->velocity, n));
    XMFLOAT3 vTan = Sub(s.body->velocity, newN);
    float     mu  = so.friction * s.body->friction;
    s.body->velocity = Add(newN, Scale(vTan, 1.0f - mu));
}

} // namespace SE
