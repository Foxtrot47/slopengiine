#pragma once
#include "Engine/Core/Window.h"
#include "Engine/Core/Logger.h"

namespace SE {

class Engine
{
public:
    Engine() = default;
    ~Engine() = default;

    bool Initialize(const WindowDesc& windowDesc = {});
    void Run();
    void Shutdown();

    Window& GetWindow() { return m_window; }

private:
    virtual void OnUpdate() {}

    Window m_window;
};

} // namespace SE
