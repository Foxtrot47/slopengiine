#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace SE {

class InputManager;

class ActionMap
{
public:
    // Bind a virtual-key code to a named action (call multiple times to add chords/aliases).
    void Bind(const std::string& action, int vk);
    void Unbind(const std::string& action);

    // Evaluate all actions against the current InputManager state.
    void Update(const InputManager& input);

    bool IsHeld    (const std::string& action) const;
    bool IsPressed (const std::string& action) const;  // true first frame only
    bool IsReleased(const std::string& action) const;  // true first frame only

private:
    struct ActionState
    {
        std::vector<int> keys;
        bool held     = false;
        bool pressed  = false;
        bool released = false;
    };

    std::unordered_map<std::string, ActionState> m_actions;
};

} // namespace SE
