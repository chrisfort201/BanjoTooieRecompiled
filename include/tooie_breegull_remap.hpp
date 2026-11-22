#ifndef TOOIE_BREEGULL_REMAP_HPP
#define TOOIE_BREEGULL_REMAP_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tooie::breegull {
// The N64 packet has already been built from RecompFrontend's configured
// keyboard/controller bindings. The host separately samples the *analog
// strengths* of those same logical C inputs through recompinput's profile API.
// No physical SDL axis or controller model is assumed by the translator.
struct Pad {
    uint16_t buttons;
    int8_t stick_x;
    int8_t stick_y;
};

struct LookInput {
    float left = 0.0f;
    float right = 0.0f;
    float up = 0.0f;
    float down = 0.0f;
};

// Traces confirmed 0xD1 during movement and 0xD8 during and immediately
// after shooting eggs. Keep the game-state allowlist narrow.
inline bool is_blaster_state(uint32_t state) {
    return (state >= 0xC8 && state <= 0xCC) || state == 0xD1 ||
           (state >= 0xD8 && state <= 0xDC);
}

// Standard libultra OSContPad masks.
constexpr uint16_t c_up    = 0x0008;
constexpr uint16_t c_down  = 0x0004;
constexpr uint16_t c_left  = 0x0002;
constexpr uint16_t c_right = 0x0001;
constexpr uint16_t r_aim   = 0x0010;

// Four unsigned 16-bit logical binding strengths fit in a single atomic word.
// This ensures the consumer hook sees one complete event-thread snapshot.
inline uint64_t pack_look(LookInput look) {
    const auto quantize = [](float value) -> uint64_t {
        return static_cast<uint16_t>(std::lround(
            std::clamp(value, 0.0f, 1.0f) * 65535.0f));
    };
    return quantize(look.left) | (quantize(look.right) << 16) |
           (quantize(look.up) << 32) | (quantize(look.down) << 48);
}

inline LookInput unpack_look(uint64_t word) {
    const auto read = [word](int shift) -> float {
        return static_cast<float>((word >> shift) & 0xFFFFu) / 65535.0f;
    };
    return {read(0), read(16), read(32), read(48)};
}

// Interpret the active *logical* C directions as an analog axis, including
// arbitrary frontend analog bindings or full-strength digital buttons/keys.
// Raw right-stick values are never used. With both directions held, the
// strengths subtract; symmetric conflicting inputs cancel.
inline int look_axis(float negative, float positive) {
    constexpr float deadzone = 5200.0f / 32767.0f;
    constexpr int n64_max = 80;
    const float value = std::clamp(positive, 0.0f, 1.0f) -
                        std::clamp(negative, 0.0f, 1.0f);
    const float magnitude = std::abs(value);
    if (magnitude <= deadzone) return 0;
    const float fraction = (magnitude - deadzone) / (1.0f - deadzone);
    const int scaled = std::clamp(static_cast<int>(std::lround(fraction * n64_max)),
                                  0, n64_max);
    return (value < 0.0f) ? -scaled : scaled;
}

inline Pad remap(Pad pad, LookInput look, bool invert_vertical_look = true) {
    constexpr int left_deadzone = 22;
    const uint16_t original_buttons = pad.buttons;
    const bool precision_aim = (original_buttons & r_aim) != 0;

    // The original game uses N64 analog X to turn and C-left/right to strafe.
    // Use frontend-mapped C strengths for yaw; translate original N64 analog X
    // into strafe buttons. The N64 stick's Y value is kept for normal movement.
    const int yaw = look_axis(look.left, look.right);
    const int left_x = pad.stick_x;
    pad.buttons &= static_cast<uint16_t>(~(c_left | c_right));
    if (left_x < -left_deadzone) pad.buttons |= c_left;
    if (left_x >  left_deadzone) pad.buttons |= c_right;
    pad.stick_x = static_cast<int8_t>(yaw);

    if (precision_aim) {
        // R-held precision mode uses N64 analog Y for pitch. The modern option
        // supplies that pitch from the SAME logical C-up/down look bindings,
        // regardless of their physical controller/keyboard assignment.
        // No game movement-state or animation behavior is changed here.
        int pitch = look_axis(look.down, look.up);
        if (!invert_vertical_look) pitch = -pitch;
        pad.stick_y = static_cast<int8_t>(pitch);
    } else if (!invert_vertical_look) {
        // Ordinary Breegull look still uses the game's digital C-up/down path.
        // Only invert those bits when the player chooses standard FPS pitch.
        const uint16_t vertical = original_buttons & (c_up | c_down);
        pad.buttons &= static_cast<uint16_t>(~(c_up | c_down));
        if ((vertical & c_up) != 0) pad.buttons |= c_down;
        if ((vertical & c_down) != 0) pad.buttons |= c_up;
    }
    return pad;
}
} // namespace tooie::breegull

#endif
