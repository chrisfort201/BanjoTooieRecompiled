// Optional third-person camera adjustments. This intentionally does not change
// C-button state: the calls below come only from the game's manual camera turn
// routine (see CAMERA_PATCH.md for the verified call chain).
#include "tooie_camera_options.h"
#include "tooie_camera_settings.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {
struct CameraOptions {
    bool invert_horizontal = false;
    float rotation_speed = 1.0f;
    bool debug_trace = false;
    uint32_t speed_generation = 1;
};

std::string trim(std::string text) {
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

CameraOptions read_options() {
    CameraOptions options;
    std::ifstream file("camera.ini"); // Legacy migration only; never polled.
    std::string line;
    while (std::getline(file, line)) {
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) line.resize(comment);
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = lowercase(trim(line.substr(0, eq)));
        const std::string value = lowercase(trim(line.substr(eq + 1)));
        if (key == "invert_horizontal") {
            if (value == "true" || value == "on" || value == "yes" || value == "1") {
                options.invert_horizontal = true;
            } else if (value == "false" || value == "off" || value == "no" || value == "0") {
                options.invert_horizontal = false;
            }
        } else if (key == "debug_trace") {
            options.debug_trace = value == "true" || value == "on" ||
                                  value == "yes" || value == "1";
        } else if (key == "rotation_speed") {
            char* end = nullptr;
            const float parsed = std::strtof(value.c_str(), &end);
            if (end != value.c_str() && *end == '\0' && std::isfinite(parsed) &&
                parsed >= 1.0f && parsed <= 4.0f) {
                options.rotation_speed = parsed;
            }
        }
    }
    return options;
}

// The UI lives on a different thread from the recompiled game. The speed
// multiplier and its generation are one atomic word, so a live setting change
// never pairs the new multiplier with a previously tagged transition.
std::atomic<bool> invert_horizontal{false};
std::atomic<bool> debug_trace{false};
constexpr uint64_t default_speed_state = (uint64_t{1} << 32) | uint32_t{0x3F800000};
std::atomic<uint64_t> speed_state{default_speed_state};

struct SpeedSnapshot {
    float multiplier;
    uint32_t generation;
};
SpeedSnapshot speed_snapshot() {
    const uint64_t packed = speed_state.load(std::memory_order_acquire);
    return {std::bit_cast<float>(static_cast<uint32_t>(packed)),
            static_cast<uint32_t>(packed >> 32)};
}
CameraOptions options() {
    const SpeedSnapshot speed = speed_snapshot();
    return {invert_horizontal.load(std::memory_order_relaxed),
            speed.multiplier,
            debug_trace.load(std::memory_order_relaxed),
            speed.generation};
}
} // namespace

namespace tooie::camera {
Settings legacy_ini_defaults() {
    const CameraOptions from_ini = read_options();
    return {from_ini.invert_horizontal, from_ini.rotation_speed, from_ini.debug_trace};
}
void set_invert_horizontal(bool enabled) {
    invert_horizontal.store(enabled, std::memory_order_relaxed);
}
void set_rotation_speed(float multiplier) {
    if (!std::isfinite(multiplier) || multiplier < 1.0f || multiplier > 4.0f) return;
    const uint32_t bits = std::bit_cast<uint32_t>(multiplier);
    uint64_t previous = speed_state.load(std::memory_order_relaxed);
    while (static_cast<uint32_t>(previous) != bits) {
        // A change from 4x -> 1x -> 4x must not revive an old camera tag.
        const uint64_t next =
            (static_cast<uint64_t>(static_cast<uint32_t>(previous >> 32) + 1) << 32) | bits;
        if (speed_state.compare_exchange_weak(previous, next,
                                              std::memory_order_release,
                                              std::memory_order_relaxed)) return;
    }
}
void set_debug_trace(bool enabled) {
    debug_trace.store(enabled, std::memory_order_relaxed);
}
Settings snapshot() {
    const CameraOptions current = options();
    return {current.invert_horizontal, current.rotation_speed, current.debug_trace};
}
} // namespace tooie::camera

extern "C" uint32_t tooie_camera_manual_yaw_bits(uint32_t original_bits) {
    // Exact neutral fast path: no writes to guest registers or memory.
    if (!invert_horizontal.load(std::memory_order_relaxed)) return original_bits;
    return original_bits ^ 0x80000000u;
}

namespace {
// Manual yaw requests and the corresponding per-frame controller run on the
// game's recompilation thread. No guest pointers are retained across threads.
struct ManualYawState {
    uint32_t camera = 0;
    uint32_t transition = 0;
    uint32_t target_bits = 0;
    uint32_t pending_camera = 0;
    uint32_t pending_target_bits = 0;
    bool pending = false;
    float active_factor = 1.0f;
    uint32_t generation = 0;
    uint32_t pending_generation = 0;
};

thread_local ManualYawState manual_yaw;

// The controller constants are MIPS float bits, not host-side native pointers.
uint32_t scale_float_bits(uint32_t bits, float factor) {
    float original;
    std::memcpy(&original, &bits, sizeof original);
    const float scaled = original * factor;
    uint32_t result;
    std::memcpy(&result, &scaled, sizeof result);
    return result;
}

gpr guest_address(uint32_t ptr) {
    return static_cast<gpr>(static_cast<int32_t>(ptr));
}

bool is_our_transition(uint8_t* rdram, uint32_t camera, uint32_t generation) {
    if (!camera || manual_yaw.camera != camera ||
        manual_yaw.generation != generation) return false;
    const auto state = static_cast<uint32_t>(MEM_W(0x60, guest_address(camera)));
    if (state != manual_yaw.transition || !state) return false;
    const gpr p = guest_address(state);
    // 0xC is the controller type (1=manual yaw), 0xE the request priority.
    return static_cast<int16_t>(MEM_H(0xC, p)) == 1 &&
           static_cast<int16_t>(MEM_H(0xE, p)) == 2 &&
           static_cast<uint32_t>(MEM_W(0x10, p)) == manual_yaw.target_bits;
}
} // namespace


// These host-side helpers are invoked by N64Recomp's configured hook points.
// They must preserve all registers except the specifically modified argument.
extern "C" void tooie_camera_hook_invert(uint8_t*, recomp_context* ctx) {
    if (!invert_horizontal.load(std::memory_order_relaxed)) return;
    ctx->r5 = static_cast<int32_t>(static_cast<uint32_t>(ctx->r5) ^ 0x80000000u);
}

// Implemented in camera_diagnostics.cpp; disabled by default.
extern "C" void tooie_camera_trace_before(uint8_t*, uint32_t, uint32_t, uint32_t, float);
extern "C" void tooie_camera_trace_motion_parameters(float, float, float, float);
extern "C" void tooie_camera_trace_after(uint8_t*);
extern "C" void tooie_camera_trace_tick(uint8_t*, uint32_t, int);

extern "C" void tooie_camera_hook_request(uint8_t* rdram, recomp_context* ctx) {
    // The unmodified path performs no camera state inspection or register writes.
    const CameraOptions settings = options();
    if (settings.rotation_speed == 1.0f && !settings.debug_trace) return;

    // The 45-degree request is unmodified; only the downstream controller's
    // velocity/acceleration constants change for a tagged accepted request.
    if (settings.rotation_speed > 1.0f) {
        manual_yaw.pending_camera = static_cast<uint32_t>(ctx->r4);
        manual_yaw.pending_target_bits = static_cast<uint32_t>(ctx->r5);
        manual_yaw.pending_generation = settings.speed_generation;
        manual_yaw.pending = true;
    } else {
        manual_yaw.pending = false;
    }
    if (settings.debug_trace) {
        tooie_camera_trace_before(rdram, static_cast<uint32_t>(ctx->r4),
                                  static_cast<uint32_t>(ctx->r5),
                                  static_cast<uint32_t>(ctx->r6), settings.rotation_speed);
    }
}

extern "C" void tooie_camera_hook_transition_setup(uint8_t*, recomp_context* ctx) {
    if (speed_snapshot().multiplier == 1.0f) return;
    // Any competing original transition replaces the tagged manual request.
    if (manual_yaw.camera == static_cast<uint32_t>(ctx->r4)) {
        manual_yaw.camera = 0;
        manual_yaw.transition = 0;
        manual_yaw.target_bits = 0;
    }
}

extern "C" void tooie_camera_hook_request_complete(uint8_t* rdram, recomp_context*) {
    // Do not read guest camera memory unless a modified manual yaw was issued.
    if (manual_yaw.pending) {
        const SpeedSnapshot speed = speed_snapshot();
        if (speed.multiplier > 1.0f &&
            manual_yaw.pending_generation == speed.generation) {
            const uint32_t camera = manual_yaw.pending_camera;
            if (camera) {
                const auto state = static_cast<uint32_t>(MEM_W(0x60, guest_address(camera)));
                if (state) {
                    const gpr p = guest_address(state);
                    if (static_cast<int16_t>(MEM_H(0xC, p)) == 1 &&
                        static_cast<int16_t>(MEM_H(0xE, p)) == 2 &&
                        static_cast<uint32_t>(MEM_W(0x10, p)) == manual_yaw.pending_target_bits) {
                        manual_yaw.camera = camera;
                        manual_yaw.transition = state;
                        manual_yaw.target_bits = manual_yaw.pending_target_bits;
                        manual_yaw.generation = speed.generation;
                    }
                }
            }
        }
        manual_yaw.pending = false;
    }
    if (debug_trace.load(std::memory_order_relaxed)) tooie_camera_trace_after(rdram);
}

extern "C" void tooie_camera_hook_motion_enter(uint8_t* rdram, recomp_context* ctx) {
    manual_yaw.active_factor = 1.0f;
    const SpeedSnapshot speed = speed_snapshot();
    if (speed.multiplier <= 1.0f) return; // No guest reads in neutral mode.
    // Capture one multiplier for both speed and acceleration in this update.
    if (is_our_transition(rdram, static_cast<uint32_t>(ctx->r16), speed.generation)) {
        manual_yaw.active_factor = speed.multiplier;
    }
}

extern "C" void tooie_camera_hook_motion_leave(uint8_t*, recomp_context*) {
    manual_yaw.active_factor = 1.0f;
}

extern "C" void tooie_camera_hook_motion_speed(uint8_t*, recomp_context* ctx) {
    const float factor = manual_yaw.active_factor;
    if (factor == 1.0f) return;
    ctx->f6.u32l = scale_float_bits(ctx->f6.u32l, factor);
}

extern "C" void tooie_camera_hook_motion_acceleration(uint8_t*, recomp_context* ctx) {
    const float factor = manual_yaw.active_factor;
    if (factor == 1.0f) return;
    ctx->r6 = static_cast<int32_t>(scale_float_bits(static_cast<uint32_t>(ctx->r6), factor));
    ctx->r7 = static_cast<int32_t>(scale_float_bits(static_cast<uint32_t>(ctx->r7), factor));
    if (debug_trace.load(std::memory_order_relaxed)) {
        const uint32_t velocity_bits = ctx->f6.u32l;
        float max_velocity;
        std::memcpy(&max_velocity, &velocity_bits, sizeof max_velocity);
        const uint32_t accel_bits = static_cast<uint32_t>(ctx->r6);
        const uint32_t brake_bits = static_cast<uint32_t>(ctx->r7);
        float acceleration, braking;
        std::memcpy(&acceleration, &accel_bits, sizeof acceleration);
        std::memcpy(&braking, &brake_bits, sizeof braking);
        tooie_camera_trace_motion_parameters(factor, max_velocity, acceleration, braking);
    }
}

extern "C" void tooie_camera_hook_tick(uint8_t* rdram, recomp_context* ctx, int after) {
    if (debug_trace.load(std::memory_order_relaxed)) {
        tooie_camera_trace_tick(rdram, static_cast<uint32_t>(ctx->r16), after);
    }
}
