#include "Engine/Scene/Camera/CameraController.h"
#include "Engine/Input/InputManager.h"

namespace SE {

void CameraController::Update(float dt, InputManager& input, CameraComponent& cam,
                               HWND hwnd, bool mouseBlocked)
{
    if (input.IsKeyPressed(VK_TAB))
    {
        if (m_mode == Mode::Orbit)
        {
            // Seed FPS position and orientation from current arcball state.
            fps.position = cam.eye;
            fps.yawDeg   = orbit.yawDeg;
            fps.pitchDeg = orbit.pitchDeg;
            m_mode = Mode::FPS;
        }
        else
        {
            m_mode = Mode::Orbit;
        }
    }

    if (m_mode == Mode::Orbit)
        orbit.Update(input, cam, mouseBlocked);
    else
        fps.Update(dt, input, cam, hwnd, mouseBlocked);
}

} // namespace SE
