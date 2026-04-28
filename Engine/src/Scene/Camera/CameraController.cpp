#include "Engine/Scene/Camera/CameraController.h"
#include "Engine/Input/InputManager.h"
#include <cmath>

namespace SE {

static void CaptureMouse(HWND hwnd, bool& capturing, bool& skipFirst)
{
    capturing  = true;
    skipFirst  = true;
    ShowCursor(FALSE);
    // Warp to centre immediately so first delta is zero.
    RECT r; GetClientRect(hwnd, &r);
    POINT c = { (r.right - r.left) / 2, (r.bottom - r.top) / 2 };
    ClientToScreen(hwnd, &c);
    SetCursorPos(c.x, c.y);
}

static void ReleaseMouse(bool& capturing)
{
    capturing = false;
    ShowCursor(TRUE);
}

// Reads mouse delta from screen-centre warp technique (same as old FPS code).
static void PollMouseDelta(HWND hwnd, InputManager& input,
                           bool& skipFirst, float& outDX, float& outDY)
{
    RECT r; GetClientRect(hwnd, &r);
    int32_t cx = (r.right  - r.left) / 2;
    int32_t cy = (r.bottom - r.top)  / 2;

    outDX = outDY = 0.0f;
    if (!skipFirst)
    {
        POINT cursor; GetCursorPos(&cursor); ScreenToClient(hwnd, &cursor);
        outDX = static_cast<float>(cursor.x - cx);
        outDY = static_cast<float>(cursor.y - cy);
    }
    skipFirst = false;

    input.IgnoreMouseMoveAt(cx, cy);
    POINT screen = { cx, cy }; ClientToScreen(hwnd, &screen);
    SetCursorPos(screen.x, screen.y);
}

// -------------------------------------------------------------------------

void CameraController::Update(float dt, InputManager& input, CameraComponent& cam,
                               HWND hwnd, bool mouseBlocked)
{
    if (input.IsKeyPressed(VK_TAB))
    {
        if (m_mode == Mode::FreeFly)
        {
            // Carry eye position + orientation into FPS state.
            fps.yawDeg   = freeFly.yawDeg;
            fps.pitchDeg = freeFly.pitchDeg;
            // Release mouse capture so FPS can recapture with RMB.
            if (m_capturing) ReleaseMouse(m_capturing);
            m_mode = Mode::FPS;
        }
        else // FPS → FreeFly
        {
            // Carry current camera eye + orientation back into free-fly.
            freeFly.eye      = cam.eye;
            freeFly.yawDeg   = fps.yawDeg;
            freeFly.pitchDeg = fps.pitchDeg;
            if (m_capturing) ReleaseMouse(m_capturing);
            m_mode = Mode::FreeFly;
        }
    }

    if (m_mode == Mode::FreeFly)
        UpdateFreeFly(dt, input, cam, hwnd, mouseBlocked);
    else
        UpdateFPS(input, cam, hwnd, mouseBlocked);
}

void CameraController::UpdateFreeFly(float dt, InputManager& input, CameraComponent& cam,
                                      HWND hwnd, bool mouseBlocked)
{
    using namespace DirectX;

    bool rmbDown = input.IsKeyDown(VK_RBUTTON) && !mouseBlocked;

    if (rmbDown && !m_capturing)
        CaptureMouse(hwnd, m_capturing, m_skipFirstFrame);
    else if ((!rmbDown || mouseBlocked) && m_capturing)
        ReleaseMouse(m_capturing);

    if (m_capturing)
    {
        float dx, dy;
        PollMouseDelta(hwnd, input, m_skipFirstFrame, dx, dy);
        freeFly.yawDeg   += dx * freeFly.sensitivity;
        freeFly.pitchDeg -= dy * freeFly.sensitivity;
        if (freeFly.pitchDeg < -89.0f) freeFly.pitchDeg = -89.0f;
        if (freeFly.pitchDeg >  89.0f) freeFly.pitchDeg =  89.0f;
    }

    float yaw   = XMConvertToRadians(freeFly.yawDeg);
    float pitch = XMConvertToRadians(freeFly.pitchDeg);

    XMFLOAT3 forward = {
        sinf(yaw) * cosf(pitch),
        sinf(pitch),
        cosf(yaw) * cosf(pitch)
    };
    // Right is always horizontal (no roll).
    XMFLOAT3 right = { cosf(yaw), 0.0f, -sinf(yaw) };

    float speed = freeFly.moveSpeed;
    if (input.IsKeyDown(VK_SHIFT)) speed *= 3.0f;

    XMVECTOR eye = XMLoadFloat3(&freeFly.eye);
    XMVECTOR fwd = XMLoadFloat3(&forward);
    XMVECTOR rgt = XMLoadFloat3(&right);
    XMVECTOR up  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    if (input.IsKeyDown('W')) eye = XMVectorAdd(eye, XMVectorScale(fwd, speed * dt));
    if (input.IsKeyDown('S')) eye = XMVectorSubtract(eye, XMVectorScale(fwd, speed * dt));
    if (input.IsKeyDown('D')) eye = XMVectorAdd(eye, XMVectorScale(rgt, speed * dt));
    if (input.IsKeyDown('A')) eye = XMVectorSubtract(eye, XMVectorScale(rgt, speed * dt));
    if (input.IsKeyDown('E')) eye = XMVectorAdd(eye, XMVectorScale(up, speed * dt));
    if (input.IsKeyDown('Q')) eye = XMVectorSubtract(eye, XMVectorScale(up, speed * dt));

    XMStoreFloat3(&freeFly.eye, eye);

    cam.eye    = freeFly.eye;
    cam.target = {
        freeFly.eye.x + forward.x,
        freeFly.eye.y + forward.y,
        freeFly.eye.z + forward.z
    };
    cam.up = { 0.0f, 1.0f, 0.0f };
}

void CameraController::UpdateFPS(InputManager& input, CameraComponent& /*cam*/,
                                  HWND hwnd, bool mouseBlocked)
{
    bool rmbDown = input.IsKeyDown(VK_RBUTTON) && !mouseBlocked;

    if (rmbDown && !m_capturing)
        CaptureMouse(hwnd, m_capturing, m_skipFirstFrame);
    else if ((!rmbDown || mouseBlocked) && m_capturing)
        ReleaseMouse(m_capturing);

    if (m_capturing)
    {
        float dx, dy;
        PollMouseDelta(hwnd, input, m_skipFirstFrame, dx, dy);
        fps.yawDeg   += dx * fps.sensitivity;
        fps.pitchDeg -= dy * fps.sensitivity;
        if (fps.pitchDeg < -89.0f) fps.pitchDeg = -89.0f;
        if (fps.pitchDeg >  89.0f) fps.pitchDeg =  89.0f;
    }
}

} // namespace SE
