#pragma once
#include <cmath>
#include <cfloat>
#include "Engine/Physics/AABB.h"
#include "Engine/Physics/Sphere.h"
#include "Engine/Physics/Ray.h"
#include "Engine/Physics/Plane.h"
#include "Engine/Physics/OBB.h"

namespace SE {

// AABB vs AABB
inline bool Intersects(const AABB& a, const AABB& b)
{
    return a.Overlaps(b);
}

// Sphere vs Sphere
inline bool Intersects(const Sphere& a, const Sphere& b)
{
    return a.Overlaps(b);
}

// AABB vs Sphere — closest-point test
inline bool Intersects(const AABB& box, const Sphere& s)
{
    using namespace DirectX;
    // Clamp sphere centre to box, compute squared distance.
    float dx = s.center.x < box.min.x ? box.min.x - s.center.x
             : s.center.x > box.max.x ? s.center.x - box.max.x : 0.0f;
    float dy = s.center.y < box.min.y ? box.min.y - s.center.y
             : s.center.y > box.max.y ? s.center.y - box.max.y : 0.0f;
    float dz = s.center.z < box.min.z ? box.min.z - s.center.z
             : s.center.z > box.max.z ? s.center.z - box.max.z : 0.0f;
    return (dx * dx + dy * dy + dz * dz) <= s.radius * s.radius;
}

// Ray vs AABB — slab method.  tMin is distance to entry point.
inline bool Intersects(const Ray& ray, const AABB& box, float& tMin)
{
    tMin = 0.0f;
    float tMax = FLT_MAX;

    const float* orig = &ray.origin.x;
    const float* dir  = &ray.direction.x;
    const float* bmin = &box.min.x;
    const float* bmax = &box.max.x;

    for (int i = 0; i < 3; ++i)
    {
        if (fabsf(dir[i]) < 1e-8f)
        {
            if (orig[i] < bmin[i] || orig[i] > bmax[i]) return false;
        }
        else
        {
            float invD = 1.0f / dir[i];
            float t0   = (bmin[i] - orig[i]) * invD;
            float t1   = (bmax[i] - orig[i]) * invD;
            if (t0 > t1) { float tmp = t0; t0 = t1; t1 = tmp; }
            tMin = t0 > tMin ? t0 : tMin;
            tMax = t1 < tMax ? t1 : tMax;
            if (tMin > tMax) return false;
        }
    }
    return true;
}

// Ray vs Sphere — geometric solution.  t is distance to nearest hit.
inline bool Intersects(const Ray& ray, const Sphere& s, float& t)
{
    using namespace DirectX;
    XMVECTOR oc  = XMVectorSubtract(XMLoadFloat3(&ray.origin), XMLoadFloat3(&s.center));
    XMVECTOR dir = XMLoadFloat3(&ray.direction);
    float    a   = XMVectorGetX(XMVector3Dot(dir, dir));
    float    b   = 2.0f * XMVectorGetX(XMVector3Dot(oc, dir));
    float    c   = XMVectorGetX(XMVector3Dot(oc, oc)) - s.radius * s.radius;
    float    disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;
    t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t < 0.0f) t = (-b + sqrtf(disc)) / (2.0f * a);
    return t >= 0.0f;
}

// Ray vs Plane.  t is distance along ray to intersection.
inline bool Intersects(const Ray& ray, const Plane& plane, float& t)
{
    using namespace DirectX;
    float denom = XMVectorGetX(
        XMVector3Dot(XMLoadFloat3(&ray.direction), XMLoadFloat3(&plane.normal)));
    if (fabsf(denom) < 1e-8f) return false;
    XMFLOAT3 orig = ray.origin;
    float num = plane.d - XMVectorGetX(
        XMVector3Dot(XMLoadFloat3(&orig), XMLoadFloat3(&plane.normal)));
    t = num / denom;
    return t >= 0.0f;
}

// OBB vs OBB — Separating Axis Theorem (15 axes: 3+3 face normals, 9 edge cross products).
// R[i][j] = dot(a.axes[i], b.axes[j]); t[i] = dot(D, a.axes[i]) where D = b.center - a.center.
inline bool Intersects(const OBB& a, const OBB& b)
{
    using namespace DirectX;

    XMVECTOR D = XMVectorSubtract(XMLoadFloat3(&b.center), XMLoadFloat3(&a.center));

    float ae[3] = { a.halfExtents.x, a.halfExtents.y, a.halfExtents.z };
    float be[3] = { b.halfExtents.x, b.halfExtents.y, b.halfExtents.z };

    float R[3][3], AbsR[3][3];
    for (int i = 0; i < 3; ++i)
    {
        XMVECTOR ai = XMLoadFloat3(&a.axes[i]);
        for (int j = 0; j < 3; ++j)
        {
            R[i][j]    = XMVectorGetX(XMVector3Dot(ai, XMLoadFloat3(&b.axes[j])));
            AbsR[i][j] = fabsf(R[i][j]) + 1e-8f;
        }
    }

    float t[3];
    for (int i = 0; i < 3; ++i)
        t[i] = XMVectorGetX(XMVector3Dot(D, XMLoadFloat3(&a.axes[i])));

    // Face normals of A
    for (int i = 0; i < 3; ++i)
        if (fabsf(t[i]) > ae[i] + be[0]*AbsR[i][0] + be[1]*AbsR[i][1] + be[2]*AbsR[i][2]) return false;

    // Face normals of B
    for (int j = 0; j < 3; ++j)
    {
        float tB = XMVectorGetX(XMVector3Dot(D, XMLoadFloat3(&b.axes[j])));
        if (fabsf(tB) > ae[0]*AbsR[0][j] + ae[1]*AbsR[1][j] + ae[2]*AbsR[2][j] + be[j]) return false;
    }

    // Edge cross products (a.axes[i] x b.axes[j])
    if (fabsf(t[2]*R[1][0]-t[1]*R[2][0]) > ae[1]*AbsR[2][0]+ae[2]*AbsR[1][0]+be[1]*AbsR[0][2]+be[2]*AbsR[0][1]) return false;
    if (fabsf(t[2]*R[1][1]-t[1]*R[2][1]) > ae[1]*AbsR[2][1]+ae[2]*AbsR[1][1]+be[0]*AbsR[0][2]+be[2]*AbsR[0][0]) return false;
    if (fabsf(t[2]*R[1][2]-t[1]*R[2][2]) > ae[1]*AbsR[2][2]+ae[2]*AbsR[1][2]+be[0]*AbsR[0][1]+be[1]*AbsR[0][0]) return false;
    if (fabsf(t[0]*R[2][0]-t[2]*R[0][0]) > ae[0]*AbsR[2][0]+ae[2]*AbsR[0][0]+be[1]*AbsR[1][2]+be[2]*AbsR[1][1]) return false;
    if (fabsf(t[0]*R[2][1]-t[2]*R[0][1]) > ae[0]*AbsR[2][1]+ae[2]*AbsR[0][1]+be[0]*AbsR[1][2]+be[2]*AbsR[1][0]) return false;
    if (fabsf(t[0]*R[2][2]-t[2]*R[0][2]) > ae[0]*AbsR[2][2]+ae[2]*AbsR[0][2]+be[0]*AbsR[1][1]+be[1]*AbsR[1][0]) return false;
    if (fabsf(t[1]*R[0][0]-t[0]*R[1][0]) > ae[0]*AbsR[1][0]+ae[1]*AbsR[0][0]+be[1]*AbsR[2][2]+be[2]*AbsR[2][1]) return false;
    if (fabsf(t[1]*R[0][1]-t[0]*R[1][1]) > ae[0]*AbsR[1][1]+ae[1]*AbsR[0][1]+be[0]*AbsR[2][2]+be[2]*AbsR[2][0]) return false;
    if (fabsf(t[1]*R[0][2]-t[0]*R[1][2]) > ae[0]*AbsR[1][2]+ae[1]*AbsR[0][2]+be[0]*AbsR[2][1]+be[1]*AbsR[2][0]) return false;

    return true;
}

// Ray vs OBB — transform ray to OBB local space, then slab test.
inline bool Intersects(const Ray& ray, const OBB& obb, float& t)
{
    using namespace DirectX;

    XMVECTOR d   = XMVectorSubtract(XMLoadFloat3(&ray.origin), XMLoadFloat3(&obb.center));
    XMVECTOR dir = XMLoadFloat3(&ray.direction);

    float tMin = 0.0f, tMax = FLT_MAX;

    for (int i = 0; i < 3; ++i)
    {
        XMVECTOR ax = XMLoadFloat3(&obb.axes[i]);
        float    e  = XMVectorGetX(XMVector3Dot(d,   ax));
        float    f  = XMVectorGetX(XMVector3Dot(dir, ax));
        float    he = (&obb.halfExtents.x)[i];

        if (fabsf(f) > 1e-8f)
        {
            float t1 = (-he - e) / f;
            float t2 = ( he - e) / f;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tMin) tMin = t1;
            if (t2 < tMax) tMax = t2;
            if (tMin > tMax) return false;
        }
        else if (e < -he || e > he) return false;
    }

    t = tMin;
    return tMin >= 0.0f;
}

// Ray vs OBB — same slab test but also returns the outward face normal at the hit point.
inline bool Intersects(const Ray& ray, const OBB& obb, float& t, DirectX::XMFLOAT3& hitNormal)
{
    using namespace DirectX;

    XMVECTOR d   = XMVectorSubtract(XMLoadFloat3(&ray.origin), XMLoadFloat3(&obb.center));
    XMVECTOR dir = XMLoadFloat3(&ray.direction);

    float tMin = 0.0f, tMax = FLT_MAX;
    int   hitAxis = -1;
    float hitSign = 1.0f;

    for (int i = 0; i < 3; ++i)
    {
        XMVECTOR ax = XMLoadFloat3(&obb.axes[i]);
        float    e  = XMVectorGetX(XMVector3Dot(d,   ax));
        float    f  = XMVectorGetX(XMVector3Dot(dir, ax));
        float    he = (&obb.halfExtents.x)[i];

        if (fabsf(f) > 1e-8f)
        {
            // t1 from -he face, t2 from +he face (before potential swap).
            // f > 0: ray travels in +axis, so enters from -he side → normal = -axes[i].
            float sign = (f > 0.0f) ? -1.0f : 1.0f;
            float t1 = (-he - e) / f;
            float t2 = ( he - e) / f;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tMin) { tMin = t1; hitAxis = i; hitSign = sign; }
            if (t2 < tMax) tMax = t2;
            if (tMin > tMax) return false;
        }
        else if (e < -he || e > he) return false;
    }

    if (tMin < 0.0f) return false;
    t = tMin;
    if (hitAxis >= 0)
        XMStoreFloat3(&hitNormal, XMVectorScale(XMLoadFloat3(&obb.axes[hitAxis]), hitSign));
    return true;
}

// OBB vs Sphere — closest-point test.
inline bool Intersects(const OBB& obb, const Sphere& s)
{
    using namespace DirectX;

    XMVECTOR d = XMVectorSubtract(XMLoadFloat3(&s.center), XMLoadFloat3(&obb.center));
    float sqDist = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        float proj  = XMVectorGetX(XMVector3Dot(d, XMLoadFloat3(&obb.axes[i])));
        float he    = (&obb.halfExtents.x)[i];
        float excess = proj < -he ? proj + he : proj > he ? proj - he : 0.0f;
        sqDist += excess * excess;
    }
    return sqDist <= s.radius * s.radius;
}

} // namespace SE
