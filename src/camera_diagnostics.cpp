// Optional, bounded diagnostics for camera-transition tuning. No simulation
// state is changed. The hook locations are specified in banjotooie.us.toml.
#include "recomp.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>

namespace {
struct CameraTrace {
    std::mutex mutex;
    std::ofstream file;
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    uint32_t camera = 0;
    uint32_t delta = 0;
    uint32_t rate = 0;
    unsigned requests = 0;
    unsigned sample = 0;
    bool active = false;
    bool motion_logged = false;

    CameraTrace() : file("camera-debug.csv", std::ios::app) {
        if (file) {
            file << "# camera-transition trace session\n";
            file << "event,request,sample,elapsed_ms,camera,delta,rate,"
                    "state,mode,priority,target,current,elapsed,duration,out0,out4\n";
            file.flush();
        }
    }
};

CameraTrace& trace() {
    static CameraTrace value;
    return value;
}

float as_float(uint32_t bits) {
    float value;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// MEM_W / MEM_H are the runtime's guest-RDRAM access macros. These offsets
// come from the original MIPS loads and stores in func_80112D90 and
// func_80112A84, not from a guessed host C++ struct layout.
void write_state(CameraTrace& t, uint8_t* rdram, const char* event) {
    if (!t.file || !t.camera) return;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t.started).count();
    const auto guest_camera = static_cast<gpr>(static_cast<int32_t>(t.camera));
    const auto state = static_cast<uint32_t>(MEM_W(0x60, guest_camera));

    t.file << event << ',' << t.requests << ',' << t.sample << ',' << ms
           << ",0x" << std::hex << t.camera << std::dec
           << ',' << as_float(t.delta) << ',' << as_float(t.rate)
           << ",0x" << std::hex << state << std::dec;

    if ((state & 0xE0000000u) != 0x80000000u) {
        t.file << ",,,,,,,,\n";
    } else {
        const auto p = static_cast<gpr>(static_cast<int32_t>(state));
        t.file << ',' << static_cast<int32_t>(MEM_W(0x8, p))
               << ',' << static_cast<int16_t>(MEM_H(0xE, p))
               << ',' << as_float(static_cast<uint32_t>(MEM_W(0x10, p)))
               << ',' << as_float(static_cast<uint32_t>(MEM_W(0x14, p)))
               << ',' << as_float(static_cast<uint32_t>(MEM_W(0x1C, p)))
               << ',' << as_float(static_cast<uint32_t>(MEM_W(0x20, p)))
               << ',' << as_float(static_cast<uint32_t>(MEM_W(0x0, p)))
               << ',' << as_float(static_cast<uint32_t>(MEM_W(0x4, p))) << '\n';
    }
    t.file.flush();
}
} // namespace

extern "C" void tooie_camera_trace_before(uint8_t* rdram,
                                            uint32_t camera, uint32_t delta,
                                            uint32_t rate, float multiplier) {
    if (!camera) return;
    auto& t = trace();
    std::lock_guard<std::mutex> guard(t.mutex);
    if (t.requests >= 80 || !t.file) { t.active = false; return; }
    if (t.requests == 0) {
        t.file << "# configured rotation_speed=" << multiplier << '\n';
    }
    ++t.requests;
    t.camera = camera;
    t.delta = delta;
    t.rate = rate;
    t.sample = 0;
    t.active = true;
    write_state(t, rdram, "request_before");
}

extern "C" void tooie_camera_trace_after(uint8_t* rdram) {
    auto& t = trace();
    std::lock_guard<std::mutex> guard(t.mutex);
    if (t.active) write_state(t, rdram, "request_after");
}

extern "C" void tooie_camera_trace_tick(uint8_t* rdram,
                                          uint32_t camera, int after) {
    if (!camera) return;
    auto& t = trace();
    std::lock_guard<std::mutex> guard(t.mutex);
    if (!t.active || t.camera != camera || t.sample >= 100) return;
    if (!after) ++t.sample;
    write_state(t, rdram, after ? "tick_after" : "tick_before");
}

// Once per process, expose the *actual* active controller parameters so a
// missing/mis-scoped hook can be distinguished from an ineffective multiplier.
extern "C" void tooie_camera_trace_motion_parameters(float multiplier,
            float max_velocity, float acceleration, float braking) {
    auto& t = trace();
    std::lock_guard<std::mutex> guard(t.mutex);
    if (t.motion_logged || !t.file) return;
    t.motion_logged = true;
    t.file << "# applied manual motion: speed=" << multiplier
           << " max_velocity=" << max_velocity
           << " acceleration=" << acceleration
           << " braking=" << braking << '\n';
    t.file.flush();
}
