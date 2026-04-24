#include "Engine/Core/Clock.h"

namespace SE {

void Clock::Initialize()
{
    QueryPerformanceFrequency(&m_frequency);
    QueryPerformanceCounter(&m_startTime);
    m_lastTime = m_startTime;
}

void Clock::Tick()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    float raw = static_cast<float>(now.QuadPart - m_lastTime.QuadPart)
              / static_cast<float>(m_frequency.QuadPart);

    m_lastTime   = now;
    m_deltaTime  = raw < k_MaxDelta ? raw : k_MaxDelta;
    m_totalTime  = static_cast<double>(now.QuadPart - m_startTime.QuadPart)
                 / static_cast<double>(m_frequency.QuadPart);
    m_accumulator += m_deltaTime;
    ++m_frameCount;
}

bool Clock::ShouldFixedUpdate()
{
    if (m_accumulator >= m_fixedTimeStep)
    {
        m_accumulator -= m_fixedTimeStep;
        return true;
    }
    return false;
}

} // namespace SE
