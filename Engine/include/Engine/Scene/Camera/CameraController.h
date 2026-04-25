#pragma once
#include "Engine/Scene/Camera/ArcballController.h"
#include "Engine/Scene/Camera/FPSController.h"

namespace SE {
class InputManager;

class CameraController
{
public:
    enum class Mode { Orbit, FPS };

    ArcballController orbit;
    FPSController     fps;

    // Single call per frame — handles mode switching (Tab) and delegates input.
    void Update(float dt, InputManager& input, CameraComponent& cam,
                HWND hwnd, bool mouseBlocked = false);

    Mode GetMode() const { return m_mode; }

private:
    Mode m_mode = Mode::Orbit;
};

} // namespace SE
