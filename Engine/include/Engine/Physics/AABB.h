#pragma once
#include <cfloat>
#include <DirectXMath.h>

namespace SE {

struct AABB
{
    DirectX::XMFLOAT3 min = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
    DirectX::XMFLOAT3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    static AABB FromCenterExtents(DirectX::XMFLOAT3 center, DirectX::XMFLOAT3 extents)
    {
        return { { center.x - extents.x, center.y - extents.y, center.z - extents.z },
                 { center.x + extents.x, center.y + extents.y, center.z + extents.z } };
    }

    DirectX::XMFLOAT3 Center() const
    {
        return { (min.x + max.x) * 0.5f,
                 (min.y + max.y) * 0.5f,
                 (min.z + max.z) * 0.5f };
    }

    DirectX::XMFLOAT3 Extents() const
    {
        return { (max.x - min.x) * 0.5f,
                 (max.y - min.y) * 0.5f,
                 (max.z - min.z) * 0.5f };
    }

    bool IsValid() const { return min.x <= max.x; }

    void Expand(DirectX::XMFLOAT3 p)
    {
        if (p.x < min.x) min.x = p.x;  if (p.x > max.x) max.x = p.x;
        if (p.y < min.y) min.y = p.y;  if (p.y > max.y) max.y = p.y;
        if (p.z < min.z) min.z = p.z;  if (p.z > max.z) max.z = p.z;
    }

    bool Contains(DirectX::XMFLOAT3 p) const
    {
        return p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y
            && p.z >= min.z && p.z <= max.z;
    }

    bool Overlaps(const AABB& o) const
    {
        return min.x <= o.max.x && max.x >= o.min.x
            && min.y <= o.max.y && max.y >= o.min.y
            && min.z <= o.max.z && max.z >= o.min.z;
    }
};

} // namespace SE
