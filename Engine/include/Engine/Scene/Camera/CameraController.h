#pragma once
#include <windows.h>
#include <DirectXMath.h>
#include "Engine/Scene/Camera/CameraComponent.h"

namespace SE {
class InputManager;

class CameraController
{
public:
    enum class Mode { Orbit, FPS };

    struct OrbitState
    {
        DirectX::XMFLOAT3 target      = { 0.0f, 0.0f, 0.0f };
        float             distance    = 10.0f;
        float             yawDeg      =  0.0f;
        float             pitchDeg    = -15.0f;
        float             sensitivity =  0.3f;
        float             zoomSpeed   =  1.0f;
    };

    struct FPSState
    {
        DirectX::XMFLOAT3 position    = { 0.0f, 2.0f, -10.0f };
        float             yawDeg      = 0.0f;
        float             pitchDeg    = 0.0f;
        float             moveSpeed   = 5.0f;
        float             sensitivity = 0.15f;
    };

    OrbitState orbit;
    FPSState   fps;

    void Update(float dt, InputManager& input, CameraComponent& cam,
                HWND hwnd, bool mouseBlocked = false);

    Mode GetMode() const { return m_mode; }

private:
    Mode m_mode         = Mode::Orbit;
    bool m_fpsCapturing = false;
    bool m_fpsSkipFirst = false;

    void UpdateOrbit(const InputManager& input, CameraComponent& cam, bool mouseBlocked);
    void UpdateFPS(float dt, InputManager& input, CameraComponent& cam, HWND hwnd, bool mouseBlocked);
};

} // namespace SE
