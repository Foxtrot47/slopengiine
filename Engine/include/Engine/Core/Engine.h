#pragma once
#include "Engine/Core/Window.h"
#include "Engine/Core/Logger.h"
#include "Engine/Core/Clock.h"

namespace SE {

class Engine
{
public:
    Engine() = default;
    ~Engine() = default;

    bool Initialize(const WindowDesc& windowDesc = {});
    void Run();
    void Shutdown();

    Window&      GetWindow() { return m_window; }
    const Clock& GetClock()  const { return m_clock; }

private:
    virtual void OnUpdate() {}

    Window m_window;
    Clock  m_clock;
};

} // namespace SE
