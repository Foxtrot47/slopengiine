#pragma once
#include <windows.h>
#include <DirectXMath.h>
#include "Engine/Scene/Camera/CameraComponent.h"

namespace SE {
class InputManager;

class CameraController
{
public:
    enum class Mode { FreeFly, FPS };

    struct FreeFlyState
    {
        DirectX::XMFLOAT3 eye         = { 0.0f, 4.0f, -22.0f };
        float             yawDeg      =   0.0f;
        float             pitchDeg    =   0.0f;
        float             moveSpeed   =  20.0f;
        float             sensitivity =   0.15f;
    };

    struct FPSState
    {
        float yawDeg      = 0.0f;
        float pitchDeg    = 0.0f;
        float sensitivity = 0.15f;
    };

    FreeFlyState freeFly;
    FPSState     fps;

    void Update(float dt, InputManager& input, CameraComponent& cam,
                HWND hwnd, bool mouseBlocked = false);

    Mode GetMode() const { return m_mode; }

private:
    Mode m_mode           = Mode::FreeFly;
    bool m_capturing      = false;
    bool m_skipFirstFrame = false;

    void UpdateFreeFly(float dt, InputManager& input, CameraComponent& cam,
                       HWND hwnd, bool mouseBlocked);
    void UpdateFPS    (InputManager& input, CameraComponent& cam,
                       HWND hwnd, bool mouseBlocked);
};

} // namespace SE
