#pragma once

namespace SE {

class Entity;

class Component
{
public:
    virtual ~Component() = default;
    virtual void Update(float /*dt*/) {}

    bool    enabled  = true;
    Entity* GetOwner() const { return m_owner; }

protected:
    friend class Entity;
    Entity* m_owner = nullptr;
};

} // namespace SE
