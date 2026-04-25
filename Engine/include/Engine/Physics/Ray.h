#pragma once
#include <DirectXMath.h>

namespace SE {

struct Ray
{
    DirectX::XMFLOAT3 origin    = { 0.0f, 0.0f,  0.0f };
    DirectX::XMFLOAT3 direction = { 0.0f, 0.0f,  1.0f }; // must be normalised

    DirectX::XMFLOAT3 PointAt(float t) const
    {
        return { origin.x + direction.x * t,
                 origin.y + direction.y * t,
                 origin.z + direction.z * t };
    }

    // Build a world-space ray from a normalised device coordinate [-1,1].
    // Pass XMMatrixInverse(nullptr, viewProj) as invVP.
    static Ray FromNDC(float ndcX, float ndcY, DirectX::FXMMATRIX invVP)
    {
        using namespace DirectX;
        // Unproject near/far points and subtract to get direction.
        XMVECTOR near_ = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invVP);
        XMVECTOR far_  = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invVP);
        XMVECTOR dir   = XMVector3Normalize(XMVectorSubtract(far_, near_));

        Ray r;
        XMStoreFloat3(&r.origin,    near_);
        XMStoreFloat3(&r.direction, dir);
        return r;
    }
};

} // namespace SE
