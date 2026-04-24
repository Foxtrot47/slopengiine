#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace SE {

class InputManager;

class ActionMap
{
public:
    // Bind a virtual-key code to a named action.
    void Bind(const std::string& action, int vk);
    // Bind a gamepad button mask (SE::GamepadButton::*) to a named action.
    void BindGamepad(const std::string& action, uint16_t buttonMask, uint32_t padIndex = 0);
    void Unbind(const std::string& action);

    // Evaluate all actions against the current InputManager state.
    void Update(const InputManager& input);

    bool IsHeld    (const std::string& action) const;
    bool IsPressed (const std::string& action) const;
    bool IsReleased(const std::string& action) const;

private:
    struct GpadBinding { uint32_t padIndex; uint16_t mask; };

    struct ActionState
    {
        std::vector<int>        keys;
        std::vector<GpadBinding> gpads;
        bool held     = false;
        bool pressed  = false;
        bool released = false;
    };

    std::unordered_map<std::string, ActionState> m_actions;
};

} // namespace SE
