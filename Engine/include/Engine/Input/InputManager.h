#pragma once
#include <windows.h>
#include <cstdint>

namespace SE {

class InputManager
{
public:
    bool Init(HWND hwnd);
    void NewFrame();   // call once per frame before OnUpdate — resets one-shot states

    // Keyboard — use Win32 virtual-key codes (VK_W, 'A', VK_SPACE, etc.)
    bool IsKeyDown    (int vk) const;  // held this frame
    bool IsKeyPressed (int vk) const;  // true only the first frame it goes down
    bool IsKeyReleased(int vk) const;  // true only the first frame it goes up

    // Mouse — deltas accumulated since last NewFrame (WM_MOUSEMOVE based)
    int32_t GetMouseDeltaX()    const { return m_mouseDX; }
    int32_t GetMouseDeltaY()    const { return m_mouseDY; }
    int32_t GetMouseWheelDelta()const { return m_mouseWheel; }
    int32_t GetMouseX()         const { return m_mouseAbsX; }
    int32_t GetMouseY()         const { return m_mouseAbsY; }

    // Passed to Window::SetInputHook
    static LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:

    bool    m_keyDown    [256] = {};
    bool    m_keyPressed [256] = {};
    bool    m_keyReleased[256] = {};

    int32_t m_mouseDX     = 0;
    int32_t m_mouseDY     = 0;
    int32_t m_mouseWheel  = 0;
    int32_t m_mouseAbsX   = 0;
    int32_t m_mouseAbsY   = 0;
    bool    m_mouseHasPos = false;
};

} // namespace SE
