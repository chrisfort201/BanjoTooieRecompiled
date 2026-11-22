// Modern Breegull Blaster input translation, outside generated game code.
// One overlay-scoped pointer hook observes the actual first-person player;
// one core input hook translates the N64 controller packet immediately before
// the game copies it into its processed input state (func_8001608C). Hooking
// the async osContGetReadData producer risks tearing/racing with that copy.
// Both hook declarations live in banjotooie.us.toml
// so a subsequent N64Recomp run recreates the calls automatically.
#include "tooie_breegull_controls.h"
#include "tooie_breegull_remap.hpp"
#include "tooie_breegull_settings.hpp"
#include "tooie_breegull_trace.hpp"
#include "recompinput/players.h"

#include <atomic>
#include <cstdint>

#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

namespace {
std::atomic<bool> enabled{false};
// One packed word avoids pairing a stale analog axis with a new device state.
// Bit 16 = SDL controller available. The low 16 bits hold signed right X.
std::atomic<uint32_t> right_axis{0};
// Shared between the first-person game-state thread and the N64 controller
// polling thread. A thread_local here made the pad hook see zero forever.
// Observe the actor even with the toggle off, so turning it on while already
// in Breegull Blaster does not depend on the overlay entrypoint running again.
// This host-side bookkeeping never changes guest state when disabled.
std::atomic<uint32_t> firstperson_player{0};

bool valid_guest_pointer(uint32_t ptr, uint32_t bytes) {
    // Tooie uses KSEG0 guest pointers for objects in the 8 MiB N64 RDRAM.
    return ptr >= 0x80000000u && ptr <= 0x80800000u - bytes;
}

gpr guest_address(uint32_t ptr) {
    return static_cast<gpr>(static_cast<int32_t>(ptr));
}

} // namespace

namespace tooie::breegull {
void set_modern_controls(bool state) {
    enabled.store(state, std::memory_order_release);
}
bool modern_controls_enabled() {
    return enabled.load(std::memory_order_acquire);
}
void sample_right_stick() {
    const bool is_enabled = enabled.load(std::memory_order_relaxed);
    if (!is_enabled && !trace_active()) {
        right_axis.store(0, std::memory_order_relaxed);
        return;
    }
    // Respect the controller already bound to player 0 by RecompFrontend.
    // Never select the first enumerated device when multiple physical/virtual
    // pads exist (e.g. DS4Windows + DualShock). Do not take ownership of it.
    SDL_GameController* controller = recompinput::players::get_player(0).controller;
    if (controller && !SDL_GameControllerGetAttached(controller)) controller = nullptr;

    // In single-player mode the frontend can use the implicit one-pad profile
    // without an explicit player assignment. The only unambiguous fallback is
    // exactly one *opened* controller. With >1, fail closed rather than
    // sample an unrelated device. No SDL_GameControllerOpen/Close calls here.
    if (!controller && recompinput::players::is_single_player_mode()) {
        SDL_GameController* only = nullptr;
        bool ambiguous = false;
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (!SDL_IsGameController(i)) continue;
            SDL_GameController* opened = SDL_GameControllerFromInstanceID(
                SDL_JoystickGetDeviceInstanceID(i));
            if (!opened || !SDL_GameControllerGetAttached(opened)) continue;
            if (only && only != opened) { ambiguous = true; break; }
            only = opened;
        }
        if (!ambiguous) controller = only;
    }
    uint32_t axis = 0;
    int16_t raw_lx = 0, raw_ly = 0, raw_rx = 0, raw_ry = 0;
    if (controller) {
        raw_lx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        raw_ly = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
        raw_rx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
        raw_ry = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);
        if (is_enabled) axis = 0x10000u | static_cast<uint16_t>(raw_rx);
    }
    if (trace_active()) trace_raw_axes(raw_lx, raw_ly, raw_rx, raw_ry);
    right_axis.store(axis, std::memory_order_release);
}
} // namespace tooie::breegull

extern "C" void tooie_breegull_hook_player(uint8_t*, recomp_context* ctx) {
    // bsfirstp_entrypoint_1 receives the player actor as a0. Keep its guest
    // pointer across host threads; the pad hook still checks the *live* player
    // state before changing any N64 input. No guest memory is read here.
    const uint32_t player = static_cast<uint32_t>(ctx->r4);
    firstperson_player.store(valid_guest_pointer(player, 0x124) ? player : 0,
                             std::memory_order_release);
}

extern "C" void tooie_breegull_hook_pad(uint8_t* rdram, recomp_context*) {
    // The normal off path has zero guest reads/writes, as before. Recording is
    // explicitly opt-in; while recording we read input even with the feature off.
    const bool active = enabled.load(std::memory_order_acquire);
    const bool recording = tooie::breegull::trace_active();
    if (!active && !recording) return;
    const gpr pad = guest_address(0x80079B40u);
    tooie::breegull::Pad original{
        static_cast<uint16_t>(MEM_H(0, pad)),
        static_cast<int8_t>(MEM_B(2, pad)),
        static_cast<int8_t>(MEM_B(3, pad)),
    };
    auto changed = original;
    const char* reason = "toggle_off";
    uint32_t state = 0;
    if (active) {
        const uint32_t player = firstperson_player.load(std::memory_order_acquire);
        if (!valid_guest_pointer(player, 0x124)) {
            reason = "player_not_observed";
        } else {
            const uint32_t state_ptr = static_cast<uint32_t>(
                MEM_W(0x120, guest_address(player)));
            if (!valid_guest_pointer(state_ptr, 8)) {
                reason = "state_ptr_invalid";
            } else {
                state = static_cast<uint32_t>(MEM_W(4, guest_address(state_ptr)));
                if (!tooie::breegull::is_blaster_state(state)) {
                    reason = "not_blaster_state";
                } else {
                    const uint32_t sampled = right_axis.load(std::memory_order_acquire);
                    if ((sampled & 0x10000u) == 0) {
                        reason = "no_sdl_controller";
                    } else {
                        reason = "translated";
                        changed = tooie::breegull::remap(original,
                            static_cast<int16_t>(sampled & 0xFFFFu));
                        MEM_H(0, pad) = changed.buttons;
                        MEM_B(2, pad) = changed.stick_x;
                    }
                }
            }
        }
    }
    if (recording) tooie::breegull::trace_packet(original, changed, state, reason);
}

extern "C" void tooie_breegull_hook_produced(uint8_t* rdram, recomp_context*) {
    if (!tooie::breegull::trace_active()) return;
    // A read-only snapshot immediately after osContGetReadData updates the
    // producer buffer. This is not the same thread as the game input consumer.
    const gpr producer = guest_address(0x80079B40u);
    tooie::breegull::trace_produced({
        static_cast<uint16_t>(MEM_H(0, producer)),
        static_cast<int8_t>(MEM_B(2, producer)),
        static_cast<int8_t>(MEM_B(3, producer)),
    });
}

extern "C" void tooie_breegull_hook_consumed(uint8_t* rdram, recomp_context*) {
    if (!tooie::breegull::trace_active()) return;
    // 0x800160D8..0x800160E4 copied OSContPad[0] into this game-side buffer.
    // This is the packet actually copied by the game, not a claim that the
    // movement state machine subsequently executed a particular action.
    const gpr copied = guest_address(0x80079B58u);
    tooie::breegull::trace_consumed({
        static_cast<uint16_t>(MEM_H(0, copied)),
        static_cast<int8_t>(MEM_B(2, copied)),
        static_cast<int8_t>(MEM_B(3, copied)),
    });
}
