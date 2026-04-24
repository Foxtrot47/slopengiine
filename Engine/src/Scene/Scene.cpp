#include "Engine/Scene/Scene.h"
#include <algorithm>

namespace SE {

Entity* Scene::CreateEntity(const std::string& name)
{
    auto    e   = std::make_unique<Entity>(m_nextID++, name);
    Entity* ptr = e.get();
    m_entities.push_back(std::move(e));
    return ptr;
}

void Scene::DestroyEntity(EntityID id)
{
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [id](const auto& e) { return e->GetID() == id; }),
        m_entities.end());
}

Entity* Scene::FindEntity(EntityID id) const
{
    for (const auto& e : m_entities)
        if (e->GetID() == id) return e.get();
    return nullptr;
}

Entity* Scene::FindEntity(const std::string& name) const
{
    for (const auto& e : m_entities)
        if (e->GetName() == name) return e.get();
    return nullptr;
}

void Scene::Update(float dt)
{
    for (auto& e : m_entities)
        if (e->active)
            e->Update(dt);
}

} // namespace SE
