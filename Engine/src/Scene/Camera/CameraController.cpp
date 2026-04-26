#include "Engine/Scene/Camera/CameraController.h"
#include "Engine/Input/InputManager.h"
#include <cmath>

namespace SE {

void CameraController::Update(float /*dt*/, InputManager& input, CameraComponent& cam,
                               HWND hwnd, bool mouseBlocked)
{
    if (input.IsKeyPressed(VK_TAB))
    {
        if (m_mode == Mode::Orbit)
        {
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
        UpdateOrbit(input, cam, mouseBlocked);
    else
        UpdateFPS(input, cam, hwnd, mouseBlocked);
}

void CameraController::UpdateOrbit(const InputManager& input, CameraComponent& cam, bool mouseBlocked)
{
    using namespace DirectX;

    if (!mouseBlocked)
    {
        if (input.IsKeyDown(VK_LBUTTON))
        {
            orbit.yawDeg   += input.GetMouseDeltaX() * orbit.sensitivity;
            orbit.pitchDeg -= input.GetMouseDeltaY() * orbit.sensitivity;
            if (orbit.pitchDeg < -89.0f) orbit.pitchDeg = -89.0f;
            if (orbit.pitchDeg >  89.0f) orbit.pitchDeg =  89.0f;
        }

        int wheel = input.GetMouseWheelDelta();
        if (wheel != 0)
        {
            orbit.distance -= wheel * orbit.zoomSpeed * 0.1f;
            if (orbit.distance < 0.5f) orbit.distance = 0.5f;
        }
    }

    float yaw   = XMConvertToRadians(orbit.yawDeg);
    float pitch = XMConvertToRadians(orbit.pitchDeg);

    XMFLOAT3 eye;
    eye.x = orbit.target.x + sinf(yaw)  * cosf(pitch) * orbit.distance;
    eye.y = orbit.target.y + sinf(pitch)               * orbit.distance;
    eye.z = orbit.target.z - cosf(yaw)  * cosf(pitch) * orbit.distance;

    cam.eye    = eye;
    cam.target = orbit.target;
    cam.up     = { 0.0f, 1.0f, 0.0f };
}

// UpdateFPS handles only mouse look (yaw/pitch).
// Camera eye/target and character movement are set by the caller (main.cpp via CharacterController).
void CameraController::UpdateFPS(InputManager& input, CameraComponent& /*cam*/,
                                  HWND hwnd, bool mouseBlocked)
{
    bool rmbDown = input.IsKeyDown(VK_RBUTTON) && !mouseBlocked;

    if (rmbDown && !m_fpsCapturing)
    {
        m_fpsCapturing = true;
        m_fpsSkipFirst = true;
        ShowCursor(FALSE);
    }
    else if ((!rmbDown || mouseBlocked) && m_fpsCapturing)
    {
        m_fpsCapturing = false;
        ShowCursor(TRUE);
    }

    if (m_fpsCapturing)
    {
        RECT    rect;
        GetClientRect(hwnd, &rect);
        int32_t cx = (rect.right  - rect.left) / 2;
        int32_t cy = (rect.bottom - rect.top)  / 2;

        if (!m_fpsSkipFirst)
        {
            POINT cursor;
            GetCursorPos(&cursor);
            ScreenToClient(hwnd, &cursor);

            fps.yawDeg   += (cursor.x - cx) * fps.sensitivity;
            fps.pitchDeg -= (cursor.y - cy) * fps.sensitivity;
            if (fps.pitchDeg < -89.0f) fps.pitchDeg = -89.0f;
            if (fps.pitchDeg >  89.0f) fps.pitchDeg =  89.0f;
        }
        m_fpsSkipFirst = false;

        input.IgnoreMouseMoveAt(cx, cy);
        POINT screen = { cx, cy };
        ClientToScreen(hwnd, &screen);
        SetCursorPos(screen.x, screen.y);
    }
}

} // namespace SE
