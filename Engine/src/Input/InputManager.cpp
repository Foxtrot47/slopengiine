#include "Engine/Input/InputManager.h"
#include "Engine/Core/Logger.h"
#include <vector>

namespace SE {

static InputManager* s_instance = nullptr;

bool InputManager::Init(HWND hwnd)
{
    s_instance = this;

    // Raw input for keyboard only — avoids conflict with ImGui's mouse raw input registration.
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
    rid.usUsage     = 0x06; // HID_USAGE_GENERIC_KEYBOARD
    rid.dwFlags     = 0;
    rid.hwndTarget  = hwnd;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
    {
        SE_LOG_ERROR("RegisterRawInputDevices (keyboard) failed: 0x%08X", GetLastError());
        return false;
    }

    SE_LOG_INFO("InputManager initialised");
    return true;
}

void InputManager::NewFrame()
{
    memset(m_keyPressed,  0, sizeof(m_keyPressed));
    memset(m_keyReleased, 0, sizeof(m_keyReleased));
    m_mouseDX    = 0;
    m_mouseDY    = 0;
    m_mouseWheel = 0;
}

bool InputManager::IsKeyDown    (int vk) const { return vk >= 0 && vk < 256 && m_keyDown[vk];     }
bool InputManager::IsKeyPressed (int vk) const { return vk >= 0 && vk < 256 && m_keyPressed[vk];  }
bool InputManager::IsKeyReleased(int vk) const { return vk >= 0 && vk < 256 && m_keyReleased[vk]; }

LRESULT InputManager::WndProcHandler(HWND /*hwnd*/, UINT msg, WPARAM wp, LPARAM lp)
{
    if (!s_instance) return 0;
    InputManager& im = *s_instance;

    switch (msg)
    {
    case WM_INPUT:
    {
        UINT size = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT,
                        nullptr, &size, sizeof(RAWINPUTHEADER));
        std::vector<BYTE> buf(size);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT,
                            buf.data(), &size, sizeof(RAWINPUTHEADER)) != size)
            break;

        const auto* raw = reinterpret_cast<const RAWINPUT*>(buf.data());
        if (raw->header.dwType != RIM_TYPEKEYBOARD) break;

        const RAWKEYBOARD& kb = raw->data.keyboard;
        uint32_t vk = kb.VKey;
        if (vk == 0 || vk >= 256) break;

        bool down = !(kb.Flags & RI_KEY_BREAK);
        if (down  && !im.m_keyDown[vk]) im.m_keyPressed[vk]  = true;
        if (!down &&  im.m_keyDown[vk]) im.m_keyReleased[vk] = true;
        im.m_keyDown[vk] = down;
        break;
    }

    case WM_MOUSEMOVE:
    {
        int32_t x = static_cast<int32_t>(LOWORD(lp));
        int32_t y = static_cast<int32_t>(HIWORD(lp));
        if (im.m_mouseHasPos)
        {
            im.m_mouseDX += x - im.m_mouseAbsX;
            im.m_mouseDY += y - im.m_mouseAbsY;
        }
        im.m_mouseAbsX   = x;
        im.m_mouseAbsY   = y;
        im.m_mouseHasPos = true;
        break;
    }

    case WM_MOUSEWHEEL:
        im.m_mouseWheel += GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
        break;
    }

    return 0;
}

} // namespace SE
