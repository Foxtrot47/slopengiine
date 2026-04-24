#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace SE {

struct WindowDesc
{
    std::wstring title  = L"FoxEngine";
    uint32_t     width  = 1280;
    uint32_t     height = 720;
};

// Optional per-message hook — return non-zero to consume the message.
using MsgHookFn = LRESULT(*)(HWND, UINT, WPARAM, LPARAM);

class Window
{
public:
    Window() = default;
    ~Window();

    bool Open(const WindowDesc& desc);
    void Close();

    // Returns false when the OS has posted WM_QUIT (user closed window).
    bool PumpMessages();

    void SetMessageHook(MsgHookFn fn) { m_msgHook   = fn; }
    void SetInputHook  (MsgHookFn fn) { m_inputHook = fn; }

    void ToggleFullscreen();

    HWND     GetHandle()      const { return m_hwnd; }
    uint32_t GetWidth()       const { return m_width; }
    uint32_t GetHeight()      const { return m_height; }
    bool     IsOpen()         const { return m_hwnd != nullptr; }
    bool     IsFullscreen()   const { return m_isFullscreen; }

    bool     IsSizeDirty()    const { return m_sizeDirty; }
    void     ClearSizeDirty()       { m_sizeDirty = false; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HWND      m_hwnd         = nullptr;
    uint32_t  m_width        = 0;
    uint32_t  m_height       = 0;
    MsgHookFn m_msgHook      = nullptr;
    MsgHookFn m_inputHook    = nullptr;

    bool      m_isFullscreen = false;
    bool      m_sizeDirty    = false;
    RECT      m_windowedRect = {};
    DWORD     m_windowedStyle= 0;
};

} // namespace SE
