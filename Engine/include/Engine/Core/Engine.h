#pragma once
#include "Engine/Core/Window.h"
#include "Engine/Core/Logger.h"
#include "Engine/Core/Clock.h"
#include "Engine/Core/ImGuiLayer.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Assets/AssetManager.h"

namespace SE {

class Engine
{
public:
    Engine() = default;
    ~Engine() = default;

    bool Initialize(const WindowDesc& windowDesc = {});
    void Run();
    void Shutdown();

    Window&              GetWindow()   { return m_window; }
    const Clock&         GetClock()    const { return m_clock; }
    Renderer&            GetRenderer() { return m_renderer; }
    ImGuiLayer&          GetImGui()    { return m_imgui; }
    InputManager&        GetInput()          { return m_input; }
    const InputManager&  GetInput()    const { return m_input; }
    AssetManager&        GetAssets()         { return m_assets; }
    ShaderLibrary&       GetShaders()        { return m_shaders; }

protected:
    virtual void OnUpdate() {}

    // Called after scene rendering, before ImGui. Default resolves MSAA → back buffer.
    // Override to insert post-process passes (resolve MSAA to RT, fullscreen quad, etc.).
    // Must leave the back buffer bound as the current RTV when done.
    virtual void OnPostProcess() { m_renderer.ResolveToBackBuffer(); }

private:
    Window        m_window;
    Clock         m_clock;
    Renderer      m_renderer;
    ImGuiLayer    m_imgui;
    InputManager  m_input;
    AssetManager  m_assets;
    ShaderLibrary m_shaders;
};

} // namespace SE
