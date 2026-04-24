#include "Engine/Scene/Entity.h"

namespace SE {

Entity::Entity(EntityID id, std::string name)
    : m_id(id), m_name(std::move(name))
{
}

void Entity::Update(float dt)
{
    for (auto& [type, comp] : m_components)
        if (comp->enabled)
            comp->Update(dt);
}

} // namespace SE
