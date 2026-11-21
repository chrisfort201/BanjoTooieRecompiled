// Optional third-person camera adjustments. This intentionally does not change
// C-button state: the calls below come only from the game's manual camera turn
// routine (see CAMERA_PATCH.md for the verified call chain).
#include "tooie_camera_options.h"

#include <algorithm>
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
    std::ifstream file("camera.ini"); // Same working directory as assets/.
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

const CameraOptions& options() {
    // Thread-safe lazy load; settings are applied on next launch, not per frame.
    static const CameraOptions value = read_options();
    return value;
}
} // namespace

extern "C" uint32_t tooie_camera_manual_yaw_bits(uint32_t original_bits) {
    // Only the two manual-turn branches pass through here (+/- 45 degrees).
    return options().invert_horizontal ? (original_bits ^ 0x80000000u) : original_bits;
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
    bool in_motion_controller = false;
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

bool is_our_transition(uint8_t* rdram, uint32_t camera) {
    if (!camera || manual_yaw.camera != camera) return false;
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
    ctx->r5 = static_cast<int32_t>(tooie_camera_manual_yaw_bits(
        static_cast<uint32_t>(ctx->r5)));
}

// Implemented in camera_diagnostics.cpp; disabled by default.
extern "C" void tooie_camera_trace_before(uint8_t*, uint32_t, uint32_t, uint32_t, float);
extern "C" void tooie_camera_trace_motion_parameters(float, float, float, float);
extern "C" void tooie_camera_trace_after(uint8_t*);
extern "C" void tooie_camera_trace_tick(uint8_t*, uint32_t, int);

extern "C" void tooie_camera_hook_request(uint8_t* rdram, recomp_context* ctx) {
    // Manual yaw's target is in $a1; leave $a2 at the original 45.0f.
    // Scaling this parameter was shown to change transition duration but not
    // the actual 75-degrees/second rate limit in the per-frame controller.
    manual_yaw.pending_camera = static_cast<uint32_t>(ctx->r4);
    manual_yaw.pending_target_bits = static_cast<uint32_t>(ctx->r5);
    manual_yaw.pending = true;
    if (options().debug_trace) {
        tooie_camera_trace_before(rdram, manual_yaw.pending_camera,
                                  manual_yaw.pending_target_bits,
                                  static_cast<uint32_t>(ctx->r6), options().rotation_speed);
    }
}

extern "C" void tooie_camera_hook_transition_setup(uint8_t*, recomp_context* ctx) {
    // Called at entry to *every* original transition request. A cinematic,
    // scripted, or other competing request invalidates our manual yaw tag;
    // the manual caller sets it anew only after its request is accepted.
    if (manual_yaw.camera == static_cast<uint32_t>(ctx->r4)) {
        manual_yaw.camera = 0;
        manual_yaw.transition = 0;
        manual_yaw.target_bits = 0;
    }
}

extern "C" void tooie_camera_hook_request_complete(uint8_t* rdram, recomp_context*) {
    if (manual_yaw.pending) {
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
                }
            }
        }
        manual_yaw.pending = false;
    }
    if (options().debug_trace) tooie_camera_trace_after(rdram);
}

extern "C" void tooie_camera_hook_motion_enter(uint8_t* rdram, recomp_context* ctx) {
    // $s0 is the camera in func_80112A84. This hook is immediately before
    // the only call to func_801127C8; it does not affect other controller users.
    manual_yaw.in_motion_controller =
        options().rotation_speed > 1.0f &&
        is_our_transition(rdram, static_cast<uint32_t>(ctx->r16));
}

extern "C" void tooie_camera_hook_motion_leave(uint8_t*, recomp_context*) {
    manual_yaw.in_motion_controller = false;
}

extern "C" void tooie_camera_hook_motion_speed(uint8_t*, recomp_context* ctx) {
    // 0x80112824 loaded the original 75.0f into $f6. The engine passes
    // this as the maximum angular velocity to func_800FF060.
    if (manual_yaw.in_motion_controller) {
        ctx->f6.u32l = scale_float_bits(ctx->f6.u32l, options().rotation_speed);
    }
}

extern "C" void tooie_camera_hook_motion_acceleration(uint8_t*, recomp_context* ctx) {
    // Original $a2/$a3 constants: +1000.0f acceleration, -560.0f braking.
    // Scaling both alongside max speed preserves their relative tuning.
    if (manual_yaw.in_motion_controller) {
        const float factor = options().rotation_speed;
        ctx->r6 = static_cast<int32_t>(scale_float_bits(static_cast<uint32_t>(ctx->r6), factor));
        ctx->r7 = static_cast<int32_t>(scale_float_bits(static_cast<uint32_t>(ctx->r7), factor));
        if (options().debug_trace) {
            uint32_t velocity_bits = ctx->f6.u32l;
            float max_velocity;
            std::memcpy(&max_velocity, &velocity_bits, sizeof max_velocity);
            uint32_t accel_bits = static_cast<uint32_t>(ctx->r6);
            uint32_t brake_bits = static_cast<uint32_t>(ctx->r7);
            float acceleration, braking;
            std::memcpy(&acceleration, &accel_bits, sizeof acceleration);
            std::memcpy(&braking, &brake_bits, sizeof braking);
            tooie_camera_trace_motion_parameters(factor, max_velocity, acceleration, braking);
        }
    }
}

extern "C" void tooie_camera_hook_tick(uint8_t* rdram, recomp_context* ctx, int after) {
    if (options().debug_trace) {
        tooie_camera_trace_tick(rdram, static_cast<uint32_t>(ctx->r16), after);
    }
}
