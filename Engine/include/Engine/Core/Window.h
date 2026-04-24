#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace SE {

struct WindowDesc
{
    std::wstring title  = L"SlopEngine";
    uint32_t     width  = 1280;
    uint32_t     height = 720;
};

class Window
{
public:
    Window() = default;
    ~Window();

    bool Open(const WindowDesc& desc);
    void Close();

    // Returns false when the OS has posted WM_QUIT (user closed window).
    bool PumpMessages();

    HWND     GetHandle()  const { return m_hwnd; }
    uint32_t GetWidth()   const { return m_width; }
    uint32_t GetHeight()  const { return m_height; }
    bool     IsOpen()     const { return m_hwnd != nullptr; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HWND     m_hwnd   = nullptr;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
};

} // namespace SE
