#include "Engine/Core/Engine.h"

namespace SE {

bool Engine::Initialize(const WindowDesc& windowDesc)
{
    Logger::Get().Initialize("slopengine.log");

    if (!m_window.Open(windowDesc))
    {
        SE_LOG_FATAL("Failed to open window");
        return false;
    }

    SE_LOG_INFO("Engine initialised — %ux%u", windowDesc.width, windowDesc.height);
    return true;
}

void Engine::Run()
{
    SE_LOG_INFO("Entering main loop");
    while (m_window.PumpMessages())
    {
        OnUpdate();
    }
    SE_LOG_INFO("Exiting main loop");
}

void Engine::Shutdown()
{
    m_window.Close();
    Logger::Get().Shutdown();
}

} // namespace SE
