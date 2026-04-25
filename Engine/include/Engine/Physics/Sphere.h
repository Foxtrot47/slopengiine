#pragma once
#include <DirectXMath.h>

namespace SE {

struct Sphere
{
    DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
    float             radius = 1.0f;

    bool Contains(DirectX::XMFLOAT3 p) const
    {
        using namespace DirectX;
        XMVECTOR d = XMVectorSubtract(XMLoadFloat3(&p), XMLoadFloat3(&center));
        return XMVectorGetX(XMVector3LengthSq(d)) <= radius * radius;
    }

    bool Overlaps(const Sphere& o) const
    {
        using namespace DirectX;
        XMVECTOR d = XMVectorSubtract(XMLoadFloat3(&o.center), XMLoadFloat3(&center));
        float    r = radius + o.radius;
        return XMVectorGetX(XMVector3LengthSq(d)) <= r * r;
    }
};

} // namespace SE
