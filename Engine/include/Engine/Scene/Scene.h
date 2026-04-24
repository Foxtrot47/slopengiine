#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Engine/Scene/Entity.h"

namespace SE {

class Scene
{
public:
    Entity* CreateEntity(const std::string& name = "Entity");
    void    DestroyEntity(EntityID id);
    Entity* FindEntity(EntityID id)               const;
    Entity* FindEntity(const std::string& name)   const;
    void    Update(float dt);

    const std::vector<std::unique_ptr<Entity>>& GetEntities() const { return m_entities; }

private:
    std::vector<std::unique_ptr<Entity>> m_entities;
    EntityID                             m_nextID = 1;
};

} // namespace SE
