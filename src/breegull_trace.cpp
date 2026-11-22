// Opt-in Breegull input capture: raw SDL axes, N64 input before/after the
// remapper, and the packet copied by Tooie's input-consumer function.
// No guest state is changed. All rows are labelled rather than interpreted
// as actions; the output documents what the game *received*, not how far
// the player moved. Key: F9; Windows shows a non-activating on-screen notice.
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

#ifdef _WIN32
#include "SDL_syswm.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

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
    bool error_notification = false;
    std::filesystem::path filename;
};
Session& session() {
    static Session session_value;
    return session_value;
}
thread_local uint64_t packet_id = 0;

#ifdef _WIN32
// A click-through, non-activating Win32 notice is positioned over the game
// window. It works even in borderless fullscreen, unlike SDL_SetWindowTitle.
// No second SDL event loop, renderer or external dependency is introduced.
// Windows-specific presentation; tracing itself remains platform-neutral.
constexpr wchar_t kOverlayClass[] = L"TooieBreegullTraceNotice";

LRESULT CALLBACK overlay_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rect{};
        GetClientRect(hwnd, &rect);
        const int kind = static_cast<int>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        HBRUSH bg = CreateSolidBrush(RGB(24, 28, 38));
        FillRect(dc, &rect, bg);
        DeleteObject(bg);
        HBRUSH dot = CreateSolidBrush(kind == 1 ? RGB(239, 68, 68) :
                                      kind == 2 ? RGB(74, 222, 128) : RGB(251, 191, 36));
        HBRUSH old = static_cast<HBRUSH>(SelectObject(dc, dot));
        Ellipse(dc, 14, 17, 27, 30);
        SelectObject(dc, old);
        DeleteObject(dot);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(247, 248, 250));
        HFONT font = CreateFontW(-17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE,
            FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HFONT previous = static_cast<HFONT>(SelectObject(dc, font));
        RECT label{35, 6, 258, 41};
        const wchar_t* message = kind == 1 ? L"REC  -  F9 to stop" :
                                 kind == 2 ? L"Input trace saved" :
                                             L"Input trace failed";
        DrawTextW(dc, message, -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, previous);
        DeleteObject(font);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}

HWND get_overlay(HWND owner) {
    static HWND overlay = nullptr;
    if (overlay) return overlay;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = overlay_proc;
    wc.hInstance = instance;
    wc.lpszClassName = kOverlayClass;
    if (!GetClassInfoW(instance, kOverlayClass, &wc) && !RegisterClassW(&wc))
        return nullptr;
    overlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        kOverlayClass, L"", WS_POPUP, 0, 0, 262, 45,
        owner, nullptr, instance, nullptr);
    if (overlay) SetLayeredWindowAttributes(overlay, 0, 238, LWA_ALPHA);
    return overlay;
}

void update_recording_notice(SDL_Window* game, int kind) {
    if (!game) return;
    SDL_SysWMinfo wm{};
    SDL_VERSION(&wm.version);
    if (!SDL_GetWindowWMInfo(game, &wm)) return;
    HWND hwnd = get_overlay(wm.info.win.window);
    if (!hwnd) return;
    // On loss of focus don't leave a topmost tooltip above other applications.
    if (kind == 0 || SDL_GetKeyboardFocus() != game) {
        ShowWindow(hwnd, SW_HIDE);
        return;
    }
    RECT game_rect{};
    if (!GetWindowRect(wm.info.win.window, &game_rect)) return;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, kind);
    SetWindowPos(hwnd, HWND_TOPMOST, game_rect.left + 22, game_rect.top + 25,
                 262, 45, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwnd, nullptr, TRUE);
}
#endif

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
           << ',' << player_state << ',' << static_cast<int>(pad.stick_x)
           << ',' << static_cast<int>(pad.stick_y)
           << ',' << pad.buttons << '\n';
}

void start(Session& s) {
    // Executable-relative, NOT the process working directory or AppData.
    char* base_path = SDL_GetBasePath();
    if (!base_path) {
        std::fprintf(stderr, "[breegull-trace] SDL_GetBasePath: %s\n", SDL_GetError());
        s.error_notification = true;
        s.stopped = clock_t::now();
        return;
    }
    const std::filesystem::path directory =
        std::filesystem::path(reinterpret_cast<const char8_t*>(base_path)) / "input-traces";
    SDL_free(base_path);
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        std::fprintf(stderr, "[breegull-trace] create directory failed: %s\n", ec.message().c_str());
        s.error_notification = true;
        s.stopped = clock_t::now();
        return;
    }
    const auto now = std::time(nullptr);
    const auto stamp = std::put_time(std::localtime(&now), "%Y%m%d-%H%M%S");
    // Include subsecond wall-clock time and process-local sequence so a new
    // process launched in the same second will not clobber an earlier CSV.
    const auto subsecond = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() % 1000;
    static unsigned session_number = 0;
    std::ostringstream name;
    name << "breegull-input-" << stamp << '-'
         << std::setw(3) << std::setfill('0') << subsecond
         << '-' << ++session_number << ".csv";
    s.filename = directory / name.str();
    s.file.open(s.filename, std::ios::out | std::ios::trunc);
    if (!s.file) {
        std::fprintf(stderr, "[breegull-trace] cannot open %s\n", s.filename.string().c_str());
        s.error_notification = true;
        s.stopped = clock_t::now();
        return;
    }
    s.began = clock_t::now();
    s.next_packet = 0;
    request_stop.store(false, std::memory_order_release);
    s.saved_notification = false;
    s.error_notification = false;
    s.file << "elapsed_ms,packet,stage,reason,SDL_LX,SDL_LY,SDL_RX,SDL_RY,"
              "player_state,N64_X,N64_Y,N64_buttons\n";
    if (!s.file) {
        std::fprintf(stderr, "[breegull-trace] header write failed\n");
        s.file.close();
        s.error_notification = true;
        s.stopped = clock_t::now();
        return;
    }
    capturing.store(true, std::memory_order_release);
    std::fprintf(stderr, "[breegull-trace] START %s\n", s.filename.string().c_str());
}

void stop(Session& s) {
    capturing.store(false, std::memory_order_release);
    bool success = static_cast<bool>(s.file);
    if (s.file.is_open()) {
        s.file.flush();
        success = success && static_cast<bool>(s.file);
        s.file.close();
        success = success && !s.file.fail();
    }
    s.stopped = clock_t::now();
    s.saved_notification = success;
    s.error_notification = !success;
    std::fprintf(stderr, "[breegull-trace] %s %s\n",
                 success ? "SAVED" : "SAVE FAILED", s.filename.string().c_str());
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
    const bool notify_saved = s.saved_notification &&
        clock_t::now() - s.stopped < std::chrono::seconds(3);
    const bool notify_error = s.error_notification &&
        clock_t::now() - s.stopped < std::chrono::seconds(4);
    // Keep the title text as a fallback on non-Windows platforms.
    if (live) SDL_SetWindowTitle(window, "Banjo-Tooie: Recompiled [REC F9 to stop]");
    else if (notify_saved) SDL_SetWindowTitle(window, "Banjo-Tooie: Recompiled [Input trace saved]");
    else if (notify_error) SDL_SetWindowTitle(window, "Banjo-Tooie: Recompiled [Input trace FAILED]");
    else SDL_SetWindowTitle(window, "Banjo-Tooie: Recompiled");
#ifdef _WIN32
    update_recording_notice(window, live ? 1 : notify_saved ? 2 : notify_error ? 3 : 0);
#endif
    if (!notify_saved) s.saved_notification = false;
    if (!notify_error) s.error_notification = false;
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
