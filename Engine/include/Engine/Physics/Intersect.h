#pragma once
#include <cmath>
#include <cfloat>
#include "Engine/Physics/AABB.h"
#include "Engine/Physics/Sphere.h"
#include "Engine/Physics/Ray.h"
#include "Engine/Physics/Plane.h"

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

} // namespace SE
