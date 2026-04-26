#pragma once
#include <DirectXMath.h>

namespace SE {

struct CharacterController
{
    float radius     = 0.4f;
    float height     = 1.8f;   // total capsule height including end caps
    float eyeHeight  = 1.6f;   // eye above feet (position.y)
    float stepHeight = 0.35f;  // max obstacle height auto-stepped over
    float slopeLimit = 45.0f;  // degrees; steeper than this = wall
    float jumpSpeed      = 7.0f;
    float gravAccel      = 20.0f;
    float moveSpeed      = 8.0f;
    bool  gravityEnabled = true;

    DirectX::XMFLOAT3 position   = { 0.0f, 0.0f, 0.0f }; // feet (bottom of capsule)
    float             velY       = 0.0f;
    bool              isGrounded = false;

    DirectX::XMFLOAT3 GetEyePosition() const
    {
        return { position.x, position.y + eyeHeight, position.z };
    }

    void Jump()
    {
        if (isGrounded) velY = jumpSpeed;
    }
};

} // namespace SE
