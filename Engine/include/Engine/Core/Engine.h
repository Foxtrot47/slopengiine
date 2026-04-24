#pragma once

namespace SE {

class Engine
{
public:
    Engine() = default;
    ~Engine() = default;

    bool Initialize();
    void Shutdown();
};

} // namespace SE
