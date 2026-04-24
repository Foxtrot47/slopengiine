#pragma once
#include <windows.h>
#include <cstdint>

namespace SE {

class Clock
{
public:
    void Initialize();

    // Call once per frame at the top of the game loop.
    void Tick();

    float    GetDeltaTime()  const { return m_deltaTime; }
    double   GetTotalTime()  const { return m_totalTime; }
    uint64_t GetFrameCount() const { return m_frameCount; }
    float    GetFPS()        const { return m_deltaTime > 0.0f ? 1.0f / m_deltaTime : 0.0f; }

    // Fixed-timestep interface for physics (M29+).
    // Call ShouldFixedUpdate() in a while loop; each true return consumes one step.
    void SetFixedTimeStep(float seconds) { m_fixedTimeStep = seconds; }
    float GetFixedTimeStep()             const { return m_fixedTimeStep; }
    bool  ShouldFixedUpdate();

private:
    LARGE_INTEGER m_frequency  = {};
    LARGE_INTEGER m_lastTime   = {};
    LARGE_INTEGER m_startTime  = {};

    float    m_deltaTime     = 0.0f;
    double   m_totalTime     = 0.0;
    uint64_t m_frameCount    = 0;

    float    m_fixedTimeStep = 1.0f / 60.0f;
    float    m_accumulator   = 0.0f;

    // Caps delta to prevent the physics spiral-of-death on long frames.
    static constexpr float k_MaxDelta = 0.25f;
};

} // namespace SE
