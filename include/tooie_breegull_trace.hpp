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
}
#endif
