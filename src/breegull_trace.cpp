// Opt-in Breegull input capture: raw SDL axes, N64 input before/after the
// remapper, and the packet copied by Tooie's input-consumer function.
// No guest state is changed. All rows are labelled rather than interpreted
// as actions; the output documents what the game *received*, not how far
// the player moved. Key: F9, indication: application window title.
#include "tooie_breegull_trace.hpp"

#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string>

namespace tooie::breegull {
namespace {
using clock_t = std::chrono::steady_clock;
std::atomic<bool> capturing{false};
std::atomic<bool> request_stop{false};
std::atomic<uint64_t> raw_axes{0};

uint64_t pack_axes(int16_t lx, int16_t ly, int16_t rx, int16_t ry) {
    return static_cast<uint64_t>(static_cast<uint16_t>(lx)) |
           static_cast<uint64_t>(static_cast<uint16_t>(ly)) << 16 |
           static_cast<uint64_t>(static_cast<uint16_t>(rx)) << 32 |
           static_cast<uint64_t>(static_cast<uint16_t>(ry)) << 48;
}

struct Session {
    std::mutex mutex;
    std::ofstream file;
    clock_t::time_point began{};
    clock_t::time_point stopped{};
    uint64_t next_packet = 0;
    bool key_down = false;
    bool saved_notification = false;
    std::filesystem::path filename;
};
Session& session() {
    static Session session_value;
    return session_value;
}
thread_local uint64_t packet_id = 0;

void print_record(Session& s, const char* stage, uint64_t serial,
                  const Pad& pad, uint32_t player_state, const char* reason) {
    if (!s.file) return;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock_t::now() - s.began).count();
    const uint64_t raw = raw_axes.load(std::memory_order_acquire);
    auto axis = [&](unsigned n) {
        return static_cast<int16_t>((raw >> (16 * n)) & 0xFFFFu);
    };
    s.file << ms << ',' << serial << ',' << stage << ',' << reason << ','
           << axis(0) << ',' << axis(1) << ',' << axis(2) << ',' << axis(3)
           << ',' << player_state << ',' << pad.stick_x << ',' << pad.stick_y
           << ',' << pad.buttons << '\n';
}

void start(Session& s) {
    const char* appdata = std::getenv("LOCALAPPDATA");
    std::filesystem::path directory = appdata && *appdata
        ? std::filesystem::path(appdata) / "BanjoTooieRecompiled" / "input-traces"
        : std::filesystem::current_path() / "input-traces";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        std::fprintf(stderr, "[breegull-trace] create directory failed: %s\n", ec.message().c_str());
        return;
    }
    const auto now = std::time(nullptr);
    const auto stamp = std::put_time(std::localtime(&now), "%Y%m%d-%H%M%S");
    // Use an additional numeric process-local counter to avoid collisions.
    static unsigned session_number = 0;
    std::ostringstream name;
    name << "breegull-input-" << stamp << '-' << ++session_number << ".csv";
    s.filename = directory / name.str();
    s.file.open(s.filename, std::ios::out | std::ios::trunc);
    if (!s.file) {
        std::fprintf(stderr, "[breegull-trace] cannot open %s\n", s.filename.string().c_str());
        return;
    }
    s.began = clock_t::now();
    s.next_packet = 0;
    request_stop.store(false, std::memory_order_release);
    s.saved_notification = false;
    s.file << "elapsed_ms,packet,stage,reason,SDL_LX,SDL_LY,SDL_RX,SDL_RY,"
              "player_state,N64_X,N64_Y,N64_buttons\n";
    capturing.store(true, std::memory_order_release);
    std::fprintf(stderr, "[breegull-trace] START %s\n", s.filename.string().c_str());
}

void stop(Session& s) {
    capturing.store(false, std::memory_order_release);
    if (s.file.is_open()) s.file.close();
    s.stopped = clock_t::now();
    s.saved_notification = true;
    std::fprintf(stderr, "[breegull-trace] SAVED %s\n", s.filename.string().c_str());
}
} // namespace

bool trace_active() {
    return capturing.load(std::memory_order_acquire);
}
void trace_raw_axes(int16_t lx, int16_t ly, int16_t rx, int16_t ry) {
    raw_axes.store(pack_axes(lx, ly, rx, ry), std::memory_order_release);
}
void trace_hotkey(SDL_Window* window) {
    auto& s = session();
    const bool down = SDL_GetKeyboardFocus() == window &&
                      SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_F9] != 0;
    const bool exhausted = request_stop.exchange(false, std::memory_order_acq_rel);
    if ((down && !s.key_down) || exhausted) {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (capturing.load(std::memory_order_acquire)) stop(s);
        else if (!exhausted) start(s);
    }
    s.key_down = down;
    const bool live = capturing.load(std::memory_order_acquire);
    if (live) {
        SDL_SetWindowTitle(window, "Banjo-Tooie: Recompiled [REC F9 to stop]");
    } else if (s.saved_notification && clock_t::now() - s.stopped < std::chrono::seconds(3)) {
        SDL_SetWindowTitle(window, "Banjo-Tooie: Recompiled [Input trace saved]");
    } else {
        s.saved_notification = false;
        SDL_SetWindowTitle(window, "Banjo-Tooie: Recompiled");
    }
}

void trace_packet(Pad before, Pad after, uint32_t state, const char* reason) {
    if (!trace_active()) return;
    auto& s = session();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (!trace_active() || !s.file) return;
    packet_id = ++s.next_packet;
    print_record(s, "incoming", packet_id, before, state, reason);
    print_record(s, "remapped", packet_id, after, state, reason);
    // Keep captures bounded even if the user forgets to stop.
    if (s.next_packet >= 18000) request_stop.store(true, std::memory_order_release);
}
void trace_produced(Pad pad) {
    if (!trace_active()) return;
    auto& s = session();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (!trace_active() || !s.file) return;
    // Producer and consumer run on different guest threads: use elapsed_ms,
    // not the consumer's packet ID, to correlate their observations.
    print_record(s, "producer_read", 0, pad, 0, "osContGetReadData");
}
void trace_consumed(Pad pad) {
    if (!trace_active()) return;
    auto& s = session();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (!trace_active() || !s.file) return;
    print_record(s, "copied_by_game", packet_id, pad, 0, "copied");
}
} // namespace tooie::breegull
