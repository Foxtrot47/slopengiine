#pragma once
#include <DirectXMath.h>
#include "Engine/Physics/AABB.h"

namespace SE {

struct Frustum
{
    // Six planes: left, right, bottom, top, near, far.
    // Each plane as (A, B, C, D) where Ax+By+Cz+D=0, normal pointing inward.
    DirectX::XMFLOAT4 planes[6];

    // Extract from a combined view-projection matrix (row-major convention).
    void ExtractFromVP(DirectX::XMMATRIX vp)
    {
        using namespace DirectX;

        // Transpose to column-major for standard Griggs-Hartmann extraction.
        XMFLOAT4X4 m;
        XMStoreFloat4x4(&m, vp);

        // Left:   row3 + row0
        planes[0] = { m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41 };
        // Right:  row3 - row0
        planes[1] = { m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41 };
        // Bottom: row3 + row1
        planes[2] = { m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42 };
        // Top:    row3 - row1
        planes[3] = { m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42 };
        // Near:   row2
        planes[4] = { m._13, m._23, m._33, m._43 };
        // Far:    row3 - row2
        planes[5] = { m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43 };

        // Normalize each plane.
        for (int i = 0; i < 6; ++i)
        {
            XMVECTOR p   = XMLoadFloat4(&planes[i]);
            XMVECTOR len = XMVector3Length(p); // length of (A,B,C)
            p = XMVectorDivide(p, len);
            XMStoreFloat4(&planes[i], p);
        }
    }

    // Returns true if the AABB is at least partially inside the frustum.
    bool TestAABB(const AABB& aabb) const
    {
        using namespace DirectX;
        XMVECTOR mn = XMLoadFloat3(&aabb.min);
        XMVECTOR mx = XMLoadFloat3(&aabb.max);

        for (int i = 0; i < 6; ++i)
        {
            XMVECTOR p = XMLoadFloat4(&planes[i]);
            // Find the AABB vertex most in the direction of the plane normal (p-vertex).
            XMVECTOR pv = XMVectorSelect(mn, mx,
                XMVectorGreaterOrEqual(p, XMVectorZero()));
            // dot(normal, pv) + d
            float d = XMVectorGetX(XMVector3Dot(p, pv)) + XMVectorGetW(p);
            if (d < 0.0f)
                return false; // entirely outside this plane
        }
        return true;
    }
};

} // namespace SE
