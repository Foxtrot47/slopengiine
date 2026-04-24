#include "Engine/Input/ActionMap.h"
#include "Engine/Input/InputManager.h"

namespace SE {

void ActionMap::Bind(const std::string& action, int vk)
{
    m_actions[action].keys.push_back(vk);
}

void ActionMap::Unbind(const std::string& action)
{
    m_actions.erase(action);
}

void ActionMap::Update(const InputManager& input)
{
    for (auto& [name, state] : m_actions)
    {
        bool anyHeld     = false;
        bool anyPressed  = false;
        bool anyReleased = false;

        for (int vk : state.keys)
        {
            if (input.IsKeyDown    (vk)) anyHeld     = true;
            if (input.IsKeyPressed (vk)) anyPressed  = true;
            if (input.IsKeyReleased(vk)) anyReleased = true;
        }

        state.held     = anyHeld;
        state.pressed  = anyPressed;
        state.released = anyReleased;
    }
}

bool ActionMap::IsHeld    (const std::string& action) const
{
    auto it = m_actions.find(action);
    return it != m_actions.end() && it->second.held;
}

bool ActionMap::IsPressed (const std::string& action) const
{
    auto it = m_actions.find(action);
    return it != m_actions.end() && it->second.pressed;
}

bool ActionMap::IsReleased(const std::string& action) const
{
    auto it = m_actions.find(action);
    return it != m_actions.end() && it->second.released;
}

} // namespace SE
