#include "Engine/Scene/FPSController.h"
#include "Engine/Input/InputManager.h"
#include <cmath>

namespace SE {

void FPSController::Update(float dt, InputManager& input, CameraComponent& cam, HWND hwnd, bool mouseBlocked)
{
    using namespace DirectX;

    bool rmbDown = input.IsKeyDown(VK_RBUTTON) && !mouseBlocked;

    if (rmbDown && !m_capturing)
    {
        m_capturing      = true;
        m_skipFirstDelta = true;
        ShowCursor(FALSE);
    }
    else if ((!rmbDown || mouseBlocked) && m_capturing)
    {
        m_capturing = false;
        ShowCursor(TRUE);
    }

    if (m_capturing)
    {
        RECT    rect;
        GetClientRect(hwnd, &rect);
        int32_t cx = (rect.right  - rect.left) / 2;
        int32_t cy = (rect.bottom - rect.top)  / 2;

        if (!m_skipFirstDelta)
        {
            // Poll cursor in client coords, compute delta from centre ourselves.
            // This bypasses WM_MOUSEMOVE entirely so SetCursorPos causes no feedback.
            POINT cursor;
            GetCursorPos(&cursor);
            ScreenToClient(hwnd, &cursor);

            yawDeg   += (cursor.x - cx) * sensitivity;
            pitchDeg -= (cursor.y - cy) * sensitivity;
            if (pitchDeg < -89.0f) pitchDeg = -89.0f;
            if (pitchDeg >  89.0f) pitchDeg =  89.0f;
        }
        m_skipFirstDelta = false;

        // Re-centre; tell InputManager to discard the resulting WM_MOUSEMOVE.
        input.IgnoreMouseMoveAt(cx, cy);
        POINT screen = { cx, cy };
        ClientToScreen(hwnd, &screen);
        SetCursorPos(screen.x, screen.y);
    }

    float yaw   = XMConvertToRadians(yawDeg);
    float pitch = XMConvertToRadians(pitchDeg);

    XMFLOAT3 fwd   = { sinf(yaw), 0.0f,  cosf(yaw) };
    XMFLOAT3 right = { cosf(yaw), 0.0f, -sinf(yaw) };

    float spd = moveSpeed * dt;
    if (input.IsKeyDown('W')) { position.x += fwd.x * spd; position.z += fwd.z * spd; }
    if (input.IsKeyDown('S')) { position.x -= fwd.x * spd; position.z -= fwd.z * spd; }
    if (input.IsKeyDown('D')) { position.x += right.x * spd; position.z += right.z * spd; }
    if (input.IsKeyDown('A')) { position.x -= right.x * spd; position.z -= right.z * spd; }
    if (input.IsKeyDown('E') || input.IsKeyDown(VK_SPACE))   position.y += spd;
    if (input.IsKeyDown('Q') || input.IsKeyDown(VK_CONTROL)) position.y -= spd;

    XMFLOAT3 look = {
        sinf(yaw) * cosf(pitch),
        sinf(pitch),
        cosf(yaw) * cosf(pitch)
    };

    cam.eye    = position;
    cam.target = { position.x + look.x, position.y + look.y, position.z + look.z };
    cam.up     = { 0.0f, 1.0f, 0.0f };
}

} // namespace SE
