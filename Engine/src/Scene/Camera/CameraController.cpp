#include "Engine/Scene/Camera/CameraController.h"
#include "Engine/Input/InputManager.h"
#include <cmath>

namespace SE {

void CameraController::Update(float dt, InputManager& input, CameraComponent& cam,
                               HWND hwnd, bool mouseBlocked)
{
    if (input.IsKeyPressed(VK_TAB))
    {
        if (m_mode == Mode::Orbit)
        {
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
        UpdateOrbit(input, cam, mouseBlocked);
    else
        UpdateFPS(dt, input, cam, hwnd, mouseBlocked);
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

void CameraController::UpdateFPS(float dt, InputManager& input, CameraComponent& cam,
                                  HWND hwnd, bool mouseBlocked)
{
    using namespace DirectX;

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

    float yaw   = XMConvertToRadians(fps.yawDeg);
    float pitch = XMConvertToRadians(fps.pitchDeg);

    XMFLOAT3 fwd   = { sinf(yaw), 0.0f,  cosf(yaw) };
    XMFLOAT3 right = { cosf(yaw), 0.0f, -sinf(yaw) };

    float spd = fps.moveSpeed * dt;
    if (input.IsKeyDown('W')) { fps.position.x += fwd.x * spd;   fps.position.z += fwd.z * spd; }
    if (input.IsKeyDown('S')) { fps.position.x -= fwd.x * spd;   fps.position.z -= fwd.z * spd; }
    if (input.IsKeyDown('D')) { fps.position.x += right.x * spd; fps.position.z += right.z * spd; }
    if (input.IsKeyDown('A')) { fps.position.x -= right.x * spd; fps.position.z -= right.z * spd; }
    if (input.IsKeyDown('E') || input.IsKeyDown(VK_SPACE))   fps.position.y += spd;
    if (input.IsKeyDown('Q') || input.IsKeyDown(VK_CONTROL)) fps.position.y -= spd;

    XMFLOAT3 look = {
        sinf(yaw) * cosf(pitch),
        sinf(pitch),
        cosf(yaw) * cosf(pitch)
    };

    cam.eye    = fps.position;
    cam.target = { fps.position.x + look.x, fps.position.y + look.y, fps.position.z + look.z };
    cam.up     = { 0.0f, 1.0f, 0.0f };
}

} // namespace SE
