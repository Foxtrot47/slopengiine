#pragma once
#include <cstdint>
#include <Xinput.h>

namespace SE {

// Mirror XINPUT_GAMEPAD_* masks — use these with IsButtonDown/Pressed/Released.
namespace GamepadButton {
    inline constexpr uint16_t DpadUp        = XINPUT_GAMEPAD_DPAD_UP;
    inline constexpr uint16_t DpadDown      = XINPUT_GAMEPAD_DPAD_DOWN;
    inline constexpr uint16_t DpadLeft      = XINPUT_GAMEPAD_DPAD_LEFT;
    inline constexpr uint16_t DpadRight     = XINPUT_GAMEPAD_DPAD_RIGHT;
    inline constexpr uint16_t Start         = XINPUT_GAMEPAD_START;
    inline constexpr uint16_t Back          = XINPUT_GAMEPAD_BACK;
    inline constexpr uint16_t LeftThumb     = XINPUT_GAMEPAD_LEFT_THUMB;
    inline constexpr uint16_t RightThumb    = XINPUT_GAMEPAD_RIGHT_THUMB;
    inline constexpr uint16_t LeftShoulder  = XINPUT_GAMEPAD_LEFT_SHOULDER;
    inline constexpr uint16_t RightShoulder = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    inline constexpr uint16_t A             = XINPUT_GAMEPAD_A;
    inline constexpr uint16_t B             = XINPUT_GAMEPAD_B;
    inline constexpr uint16_t X             = XINPUT_GAMEPAD_X;
    inline constexpr uint16_t Y             = XINPUT_GAMEPAD_Y;
}

struct GamepadState
{
    bool connected = false;

    float leftX  = 0.0f;   // [-1, 1] with dead zone applied
    float leftY  = 0.0f;
    float rightX = 0.0f;
    float rightY = 0.0f;
    float leftTrigger  = 0.0f;  // [0, 1]
    float rightTrigger = 0.0f;

    bool IsButtonDown    (uint16_t mask) const { return connected && (buttonsHeld     & mask) != 0; }
    bool IsButtonPressed (uint16_t mask) const { return connected && (buttonsPressed  & mask) != 0; }
    bool IsButtonReleased(uint16_t mask) const { return connected && (buttonsReleased & mask) != 0; }

    uint16_t buttonsHeld     = 0;
    uint16_t buttonsPressed  = 0;
    uint16_t buttonsReleased = 0;
};

} // namespace SE
