#pragma once
#include <DirectXMath.h>
#include <cmath>

namespace SE {

struct OBB
{
    DirectX::XMFLOAT3 center      = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 halfExtents = { 1.0f, 1.0f, 1.0f };
    // Local X, Y, Z axes expressed in world space (must stay orthonormal).
    DirectX::XMFLOAT3 axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };

    // Build a world-space model matrix that maps the unit cube (-1..1) to this OBB.
    DirectX::XMMATRIX GetWorldMatrix() const
    {
        using namespace DirectX;
        return XMMATRIX(
            XMVectorSetW(XMVectorScale(XMLoadFloat3(&axes[0]), halfExtents.x), 0.0f),
            XMVectorSetW(XMVectorScale(XMLoadFloat3(&axes[1]), halfExtents.y), 0.0f),
            XMVectorSetW(XMVectorScale(XMLoadFloat3(&axes[2]), halfExtents.z), 0.0f),
            XMVectorSet(center.x, center.y, center.z, 1.0f));
    }

    // Axis-aligned OBB from min/max corners.
    static OBB FromAABB(DirectX::XMFLOAT3 mn, DirectX::XMFLOAT3 mx)
    {
        OBB o;
        o.center      = { (mn.x+mx.x)*0.5f, (mn.y+mx.y)*0.5f, (mn.z+mx.z)*0.5f };
        o.halfExtents = { (mx.x-mn.x)*0.5f, (mx.y-mn.y)*0.5f, (mx.z-mn.z)*0.5f };
        return o;
    }

    // OBB rotated around the world Y axis by angleDeg degrees.
    static OBB MakeRotatedY(DirectX::XMFLOAT3 center, DirectX::XMFLOAT3 halfExtents, float angleDeg)
    {
        OBB o;
        o.center      = center;
        o.halfExtents = halfExtents;
        float a = DirectX::XMConvertToRadians(angleDeg);
        float c = cosf(a), s = sinf(a);
        o.axes[0] = {  c, 0.0f, s };   // local X in world (LH: positive yaw rotates X toward +Z)
        o.axes[1] = { 0.0f, 1.0f, 0.0f };
        o.axes[2] = { -s, 0.0f, c };
        return o;
    }
};

} // namespace SE
