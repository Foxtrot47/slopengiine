#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include "Engine/Scene/Component.h"

namespace SE {

using EntityID = uint32_t;

class Entity
{
public:
    Entity(EntityID id, std::string name);

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args)
    {
        auto comp   = std::make_unique<T>(std::forward<Args>(args)...);
        T*   ptr    = comp.get();
        comp->m_owner = this;
        m_components[std::type_index(typeid(T))] = std::move(comp);
        return ptr;
    }

    template<typename T>
    T* GetComponent() const
    {
        auto it = m_components.find(std::type_index(typeid(T)));
        return it != m_components.end() ? static_cast<T*>(it->second.get()) : nullptr;
    }

    template<typename T>
    void RemoveComponent()
    {
        m_components.erase(std::type_index(typeid(T)));
    }

    void Update(float dt);

    EntityID           GetID()   const { return m_id; }
    const std::string& GetName() const { return m_name; }

    bool active = true;

private:
    EntityID    m_id;
    std::string m_name;
    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_components;
};

} // namespace SE
