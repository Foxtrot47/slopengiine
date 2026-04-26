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

    DirectX::XMFLOAT3 position      = { 0.0f, 0.0f, 0.0f }; // feet (bottom of capsule)
    DirectX::XMFLOAT3 contactNormal = { 0.0f, 1.0f, 0.0f }; // normalized sum of contact normals this frame
    float             velY          = 0.0f;
    float             physVelX      = 0.0f; // lateral physics velocity (wall-jump impulse etc.)
    float             physVelZ      = 0.0f;
    bool              isGrounded    = false; // true if any surface contact this frame

    DirectX::XMFLOAT3 GetEyePosition() const
    {
        return { position.x, position.y + eyeHeight, position.z };
    }

    // Push off the last contact surface in its normal direction.
    void Jump()
    {
        if (!isGrounded) return;
        velY     = contactNormal.y * jumpSpeed;
        physVelX = contactNormal.x * jumpSpeed;
        physVelZ = contactNormal.z * jumpSpeed;
    }
};

} // namespace SE
