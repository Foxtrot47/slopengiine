#include "Engine/Core/ImGuiLayer.h"
#include "Engine/Core/Logger.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace SE {

bool ImGuiLayer::Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* ctx)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(hwnd))
    {
        SE_LOG_ERROR("ImGui Win32 backend init failed");
        return false;
    }
    if (!ImGui_ImplDX11_Init(device, ctx))
    {
        SE_LOG_ERROR("ImGui DX11 backend init failed");
        return false;
    }

    SE_LOG_INFO("ImGui initialised (version %s)", IMGUI_VERSION);
    return true;
}

void ImGuiLayer::Shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::BeginFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::EndFrame()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

LRESULT ImGuiLayer::WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
}

} // namespace SE
