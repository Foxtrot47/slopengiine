#pragma once
#include <windows.h>
#include <DirectXMath.h>
#include "Engine/Scene/CameraComponent.h"

namespace SE {
class InputManager;

class FPSController
{
public:
    float             moveSpeed   = 5.0f;
    float             sensitivity = 0.15f;
    float             yawDeg      = 0.0f;
    float             pitchDeg    = 0.0f;
    DirectX::XMFLOAT3 position    = { 0.0f, 2.0f, -10.0f };

    // hwnd needed to hide cursor and re-center it while looking.
    void Update(float dt, InputManager& input, CameraComponent& cam, HWND hwnd, bool mouseBlocked = false);

private:
    bool m_capturing      = false;
    bool m_skipFirstDelta = false;
};

} // namespace SE
