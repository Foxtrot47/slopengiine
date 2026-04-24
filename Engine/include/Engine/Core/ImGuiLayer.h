#pragma once
#include <windows.h>
#include <d3d11.h>

namespace SE {

class ImGuiLayer
{
public:
    bool Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* ctx);
    void Shutdown();
    void BeginFrame();
    void EndFrame();

    // Passed to Window::SetMessageHook — keeps Window.cpp free of imgui headers.
    static LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace SE
