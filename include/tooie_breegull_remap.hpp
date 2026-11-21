#ifndef TOOIE_BREEGULL_REMAP_HPP
#define TOOIE_BREEGULL_REMAP_HPP

#include <algorithm>
#include <cstdint>

namespace tooie::breegull {
// Explicit N64 inputs, not a new engine or an alternative controller profile.
// The caller supplies the left stick and button state from RecompFrontend's
// mapped N64 controller and an analog right-X reading from SDL.
struct Pad {
    uint16_t buttons;
    int8_t stick_x;
    int8_t stick_y;
};

constexpr uint16_t c_left  = 0x0008;
constexpr uint16_t c_right = 0x0004;

inline Pad remap(Pad pad, int16_t right_x) {
    // Breegull Blaster originally uses stick X to turn and C-left/C-right to
    // strafe. Translate modern twin-stick horizontal axes into that original
    // scheme; vertical left-stick movement and other buttons are unchanged.
    constexpr int left_deadzone = 22; // N64 stick units
    constexpr int right_deadzone = 5200; // signed SDL axis units
    constexpr int n64_max = 80;

    const int left_x = pad.stick_x;
    pad.buttons &= static_cast<uint16_t>(~(c_left | c_right));
    if (left_x < -left_deadzone) pad.buttons |= c_left;
    if (left_x >  left_deadzone) pad.buttons |= c_right;

    const int raw = right_x;
    const int magnitude = (raw < 0) ? -raw : raw;
    int yaw = 0;
    if (magnitude > right_deadzone) {
        // Continuous analog yaw after deadzone, with the same N64 limit the
        // standard RecompFrontend stick mapping targets.
        yaw = (magnitude - right_deadzone) * n64_max / (32767 - right_deadzone);
        yaw = std::clamp(yaw, 0, n64_max);
        if (raw < 0) yaw = -yaw;
    }
    pad.stick_x = static_cast<int8_t>(yaw);
    return pad;
}
} // namespace tooie::breegull

#endif
