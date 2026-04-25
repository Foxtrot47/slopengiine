#pragma once
#include <windows.h>
#include <cstdint>
#include "Engine/Input/GamepadState.h"

namespace SE {

static constexpr uint32_t k_MaxGamepads = 4;

class InputManager
{
public:
    bool Init(HWND hwnd);
    void NewFrame();   // call once per frame before OnUpdate — resets one-shot states

    // Keyboard — use Win32 virtual-key codes (VK_W, 'A', VK_SPACE, etc.)
    bool IsKeyDown    (int vk) const;
    bool IsKeyPressed (int vk) const;
    bool IsKeyReleased(int vk) const;

    // Mouse — deltas accumulated since last NewFrame (WM_MOUSEMOVE based)
    int32_t GetMouseDeltaX()    const { return m_mouseDX; }
    int32_t GetMouseDeltaY()    const { return m_mouseDY; }
    int32_t GetMouseWheelDelta()const { return m_mouseWheel; }
    int32_t GetMouseX()         const { return m_mouseAbsX; }
    int32_t GetMouseY()         const { return m_mouseAbsY; }

    // Gamepad — index 0-3; always safe to call even if not connected
    const GamepadState& GetGamepad(uint32_t index = 0) const;
    void SetRumble(uint32_t index, float leftMotor, float rightMotor);

    // Call before SetCursorPos so the resulting WM_MOUSEMOVE doesn't count as input delta.
    void IgnoreMouseMoveAt(int32_t clientX, int32_t clientY);

    // Passed to Window::SetInputHook
    static LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    void PollGamepads();

    bool    m_keyDown    [256] = {};
    bool    m_keyPressed [256] = {};
    bool    m_keyReleased[256] = {};

    int32_t m_mouseDX     = 0;
    int32_t m_mouseDY     = 0;
    int32_t m_mouseWheel  = 0;
    int32_t m_mouseAbsX   = 0;
    int32_t m_mouseAbsY   = 0;
    bool    m_mouseHasPos = false;

    bool    m_ignorePending = false;
    int32_t m_ignoreX       = 0;
    int32_t m_ignoreY       = 0;

    GamepadState m_gamepads[k_MaxGamepads];
    uint16_t     m_prevButtons[k_MaxGamepads] = {};
};

} // namespace SE
