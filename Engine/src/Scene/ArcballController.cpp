#include "Engine/Scene/ArcballController.h"
#include "Engine/Input/InputManager.h"
#include <cmath>

namespace SE {

void ArcballController::Update(const InputManager& input, CameraComponent& cam, bool mouseBlocked)
{
    using namespace DirectX;

    if (!mouseBlocked)
    {
        if (input.IsKeyDown(VK_LBUTTON))
        {
            yawDeg   += input.GetMouseDeltaX() * sensitivity;
            pitchDeg -= input.GetMouseDeltaY() * sensitivity;
            if (pitchDeg < -89.0f) pitchDeg = -89.0f;
            if (pitchDeg >  89.0f) pitchDeg =  89.0f;
        }

        int wheel = input.GetMouseWheelDelta();
        if (wheel != 0)
        {
            distance -= wheel * zoomSpeed * 0.1f;
            if (distance < 0.5f) distance = 0.5f;
        }
    }

    float yaw   = XMConvertToRadians(yawDeg);
    float pitch = XMConvertToRadians(pitchDeg);

    XMFLOAT3 eye;
    eye.x = target.x + sinf(yaw)  * cosf(pitch) * distance;
    eye.y = target.y + sinf(pitch)               * distance;
    eye.z = target.z - cosf(yaw)  * cosf(pitch) * distance;

    cam.eye    = eye;
    cam.target = target;
    cam.up     = { 0.0f, 1.0f, 0.0f };
}

} // namespace SE
