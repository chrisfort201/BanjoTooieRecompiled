#ifndef TOOIE_BREEGULL_PITCH_HPP
#define TOOIE_BREEGULL_PITCH_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "tooie_breegull_remap.hpp"

namespace tooie::breegull {
// Called *after* the game has chosen one of two mutually exclusive pitch
// computations in func_808068E8_bsfirstp. This is an angular increment, NOT
// an N64 digital C-button or an analog stick value.
enum class PitchPath : uint32_t {
    TargetCorrection = 0, // func_808069BC: (target-current)/6
    ManualLook = 1,       // func_808069FC: original input/smoothing result
};

// The guest state alone is not an R-aim discriminator: both R-held and normal
// Breegull can occur in CA/D1. Require the *current consumed R button* too.
inline bool is_normal_breegull_pitch(bool modern, uint32_t state,
                                    bool r_aim_held, uint32_t camera_mode,
                                    bool same_camera_object) {
    return modern && is_blaster_state(state) && !r_aim_held &&
           camera_mode == 0 && same_camera_object;
}

constexpr float min_vertical_speed = 0.5f;
constexpr float max_vertical_speed = 3.0f;

inline float adjusted_pitch_delta(float original, PitchPath path,
                                  bool modern_normal_breegull,
                                  float manual_speed, float target_angle = 0.0f) {
    // Neutral/off is bit-for-bit unchanged (including signed zero/NaNs).
    if (!modern_normal_breegull) return original;
    if (path == PitchPath::TargetCorrection) {
        // Only suppress automatic *leveling to zero*. Nonzero targets can
        // represent intentional camera changes and should still be obeyed.
        if (std::isfinite(target_angle) && std::abs(target_angle) <= 0.01f)
            return 0.0f;
        return original;
    }
    if (path == PitchPath::ManualLook && std::isfinite(original)) {
        // 1.0x is the original per-frame pitch increment. Scaling the actual
        // increment (not the C-button bits) increases maximum pitch speed.
        const float speed = std::isfinite(manual_speed)
            ? std::clamp(manual_speed, min_vertical_speed, max_vertical_speed)
            : 1.0f;
        if (speed != 1.0f) return original * speed;
    }
    return original;
}
} // namespace tooie::breegull

#endif
