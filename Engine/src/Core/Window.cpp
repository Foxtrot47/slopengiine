#include "Engine/Core/Window.h"

namespace SE {

static const wchar_t* k_ClassName = L"FoxEngineWnd";

Window::~Window()
{
    Close();
}

bool Window::Open(const WindowDesc& desc)
{
    m_width  = desc.width;
    m_height = desc.height;

    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOWFRAME);
    wc.lpszClassName = k_ClassName;
    RegisterClassExW(&wc);

    // Calculate window size so the client area matches the requested resolution.
    RECT rc = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowExW(
        0,
        k_ClassName,
        desc.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, this);  // lpParam = this for WndProc retrieval

    if (!m_hwnd)
        return false;

    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hwnd);
    return true;
}

void Window::Close()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    UnregisterClassW(k_ClassName, GetModuleHandleW(nullptr));
}

bool Window::PumpMessages()
{
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

void Window::ToggleFullscreen()
{
    if (!m_isFullscreen)
    {
        GetWindowRect(m_hwnd, &m_windowedRect);
        m_windowedStyle = GetWindowLongW(m_hwnd, GWL_STYLE);

        HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(hMon, &mi);

        SetWindowLongW(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(m_hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right  - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_FRAMECHANGED);

        m_isFullscreen = true;
    }
    else
    {
        SetWindowLongW(m_hwnd, GWL_STYLE, m_windowedStyle);
        SetWindowPos(m_hwnd, nullptr,
            m_windowedRect.left, m_windowedRect.top,
            m_windowedRect.right  - m_windowedRect.left,
            m_windowedRect.bottom - m_windowedRect.top,
            SWP_FRAMECHANGED | SWP_NOZORDER);

        m_isFullscreen = false;
    }
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }

    Window* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self && self->m_inputHook)
        self->m_inputHook(hwnd, msg, wp, lp);
    if (self && self->m_msgHook)
        if (self->m_msgHook(hwnd, msg, wp, lp)) return true;

    switch (msg)
    {
    case WM_SIZE:
        if (self && wp != SIZE_MINIMIZED)
        {
            self->m_width     = LOWORD(lp);
            self->m_height    = HIWORD(lp);
            self->m_sizeDirty = true;
        }
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_F11 && self)
            self->ToggleFullscreen();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace SE
