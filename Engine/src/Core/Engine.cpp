#include "Engine/Core/Engine.h"
#include <cstdio>

namespace SE {

bool Engine::Initialize(const WindowDesc& windowDesc)
{
    Logger::Get().Initialize("slopengine.log");
    m_clock.Initialize();

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

    float fpsTimer = 0.0f;

    while (m_window.PumpMessages())
    {
        m_clock.Tick();
        OnUpdate();

        // Update title bar with FPS once per second.
        fpsTimer += m_clock.GetDeltaTime();
        if (fpsTimer >= 1.0f)
        {
            wchar_t title[128];
            swprintf_s(title, L"SlopEngine  |  %.1f fps  |  %.2f ms",
                       m_clock.GetFPS(),
                       m_clock.GetDeltaTime() * 1000.0f);
            SetWindowTextW(m_window.GetHandle(), title);
            fpsTimer = 0.0f;
        }
    }

    SE_LOG_INFO("Exiting main loop — %llu frames, %.2fs total",
                m_clock.GetFrameCount(), m_clock.GetTotalTime());
}

void Engine::Shutdown()
{
    m_window.Close();
    Logger::Get().Shutdown();
}

} // namespace SE
