// Modern Breegull Blaster input translation, outside generated game code.
// One overlay-scoped pointer hook observes the actual first-person player;
// one core input hook translates the N64 controller packet *after* the usual
// osContGetReadData call. Both hook declarations live in banjotooie.us.toml
// so a subsequent N64Recomp run recreates the calls automatically.
#include "tooie_breegull_controls.h"
#include "tooie_breegull_remap.hpp"
#include "tooie_breegull_settings.hpp"

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
// Updated only by the recompilation thread's first-person overlay hook.
thread_local uint32_t firstperson_player = 0;

bool valid_guest_pointer(uint32_t ptr, uint32_t bytes) {
    // Tooie uses KSEG0 guest pointers for objects in the 8 MiB N64 RDRAM.
    return ptr >= 0x80000000u && ptr <= 0x80800000u - bytes;
}

gpr guest_address(uint32_t ptr) {
    return static_cast<gpr>(static_cast<int32_t>(ptr));
}

bool blaster_state(uint32_t state) {
    // Values reached by the bsfirstp overlay's state transitions. Exclude
    // ordinary baeggaim, which has a different player-state/overlay path.
    return (state >= 0xC8 && state <= 0xCC) ||
           (state >= 0xD9 && state <= 0xDC);
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
    if (!enabled.load(std::memory_order_relaxed)) {
        right_axis.store(0, std::memory_order_relaxed);
        return;
    }
    // RecompFrontend already opens SDL controllers. This is read-only; it
    // does not open, close, remap or take ownership of any controller.
    uint32_t axis = 0;
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count; ++i) {
        if (!SDL_IsGameController(i)) continue;
        const SDL_JoystickID instance = SDL_JoystickGetDeviceInstanceID(i);
        SDL_GameController* controller = SDL_GameControllerFromInstanceID(instance);
        if (!controller) continue;
        const int16_t x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
        axis = 0x10000u | static_cast<uint16_t>(x);
        break; // Single player: first active game controller.
    }
    right_axis.store(axis, std::memory_order_release);
}
} // namespace tooie::breegull

extern "C" void tooie_breegull_hook_player(uint8_t*, recomp_context* ctx) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    // bsfirstp_entrypoint_1 receives the player actor as a0. Keeping this
    // pointer is not itself a mode detector: each packet checks *live* state.
    firstperson_player = static_cast<uint32_t>(ctx->r4);
}

extern "C" void tooie_breegull_hook_pad(uint8_t* rdram, recomp_context*) {
    // Exact neutral path: no guest reads, guest writes or register mutations.
    if (!enabled.load(std::memory_order_acquire)) return;

    const uint32_t player = firstperson_player;
    if (!valid_guest_pointer(player, 0x124)) return;
    const uint32_t state_ptr = static_cast<uint32_t>(
        MEM_W(0x120, guest_address(player)));
    if (!valid_guest_pointer(state_ptr, 8)) return;
    const uint32_t state = static_cast<uint32_t>(
        MEM_W(4, guest_address(state_ptr)));
    if (!blaster_state(state)) return;

    const uint32_t sampled = right_axis.load(std::memory_order_acquire);
    if ((sampled & 0x10000u) == 0) return; // No SDL controller; preserve input.

    constexpr uint32_t pad_addr = 0x80079B40u; // controller 0, OSContPad[0]
    const gpr pad = guest_address(pad_addr);
    tooie::breegull::Pad original{
        static_cast<uint16_t>(MEM_H(0, pad)),
        static_cast<int8_t>(MEM_B(2, pad)),
        static_cast<int8_t>(MEM_B(3, pad)),
    };
    const int16_t right_x = static_cast<int16_t>(sampled & 0xFFFFu);
    const auto changed = tooie::breegull::remap(original, right_x);
    // Preserve Y, C-up/down, attack buttons and error byte exactly. The game
    // handles movement and camera motion; no custom physics or view controller.
    MEM_H(0, pad) = changed.buttons;
    MEM_B(2, pad) = changed.stick_x;
}
