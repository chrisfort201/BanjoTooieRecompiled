#ifndef TOOIE_BREEGULL_TRACE_HPP
#define TOOIE_BREEGULL_TRACE_HPP

#include "tooie_breegull_remap.hpp"
#include <cstdint>

struct SDL_Window;
namespace tooie::breegull {
// F9 records only on demand. Never modifies emulated game memory.
void trace_hotkey(SDL_Window* window);
bool trace_active();
void trace_raw_axes(int16_t lx, int16_t ly, int16_t rx, int16_t ry);
void trace_packet(Pad before, Pad after, uint32_t state, const char* reason);
void trace_produced(Pad packet);
void trace_consumed(Pad packet);

// Candidate first-person camera data from read-only native hooks. Field names
// intentionally use guest offsets: we have NOT yet proven 0x20C is rendered
// pitch. Stage: 0 before update, 1 after update/clamp, 2 before reset,
// 3 after reset. Invalid guest pointers are recorded but never dereferenced.
struct CameraSample {
    uint32_t object = 0;
    uint32_t mode = 0;
    bool valid = false;
    float angle_20c = 0.0f;
    float angle_210 = 0.0f;
    float angle_214 = 0.0f;
    float bound_1e8 = 0.0f;
    float bound_1ec = 0.0f;
    float reg_f4 = 0.0f;
    float reg_f6 = 0.0f;
    float reg_f0 = 0.0f;
};
void trace_camera(uint32_t stage, uint32_t player_state, const CameraSample& sample);
}
#endif
