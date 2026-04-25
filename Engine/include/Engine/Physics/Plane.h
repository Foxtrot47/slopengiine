#pragma once
#include <DirectXMath.h>

namespace SE {

struct Plane
{
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    float             d      = 0.0f; // dot(normal, p) = d for on-plane points

    // Positive = in front, negative = behind.
    float SignedDistance(DirectX::XMFLOAT3 p) const
    {
        using namespace DirectX;
        return XMVectorGetX(XMVector3Dot(XMLoadFloat3(&normal), XMLoadFloat3(&p))) - d;
    }

    static Plane FromPointNormal(DirectX::XMFLOAT3 point, DirectX::XMFLOAT3 n)
    {
        using namespace DirectX;
        Plane pl;
        pl.normal = n;
        pl.d = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&point)));
        return pl;
    }

    static Plane FromTriangle(DirectX::XMFLOAT3 a,
                              DirectX::XMFLOAT3 b,
                              DirectX::XMFLOAT3 c)
    {
        using namespace DirectX;
        XMVECTOR n = XMVector3Normalize(
            XMVector3Cross(
                XMVectorSubtract(XMLoadFloat3(&b), XMLoadFloat3(&a)),
                XMVectorSubtract(XMLoadFloat3(&c), XMLoadFloat3(&a))));
        XMFLOAT3 nf;
        XMStoreFloat3(&nf, n);
        return FromPointNormal(a, nf);
    }
};

} // namespace SE
