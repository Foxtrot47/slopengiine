#include "Engine/Core/Engine.h"

namespace SE {

bool Engine::Initialize(const WindowDesc& windowDesc)
{
    if (!m_window.Open(windowDesc))
        return false;
    return true;
}

void Engine::Run()
{
    while (m_window.PumpMessages())
    {
        OnUpdate();
    }
}

void Engine::Shutdown()
{
    m_window.Close();
}

} // namespace SE
