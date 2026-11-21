#include "tooie_camera_settings.hpp"
#include "tooie_camera_options.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>

extern "C" void tooie_camera_trace_before(uint8_t*, uint32_t, uint32_t, uint32_t, float) {}
extern "C" void tooie_camera_trace_motion_parameters(float, float, float, float) {}
extern "C" void tooie_camera_trace_after(uint8_t*) {}
extern "C" void tooie_camera_trace_tick(uint8_t*, uint32_t, int) {}

// Assertions must stay active in Release builds where NDEBUG is defined.
#define CHECK(expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "FAIL at line %d: %s\n", __LINE__, #expr); \
        return 1; \
    } \
} while (false)

static uint32_t bits(float v){return std::bit_cast<uint32_t>(v);}
static float from(uint32_t v){return std::bit_cast<float>(v);}
int main() {
    using namespace tooie::camera;
    auto opts=snapshot();
    CHECK(!opts.invert_horizontal && opts.rotation_speed==1.0f && !opts.debug_trace);
    recomp_context ctx{};
    ctx.r4 = 0xFFFFFFFF80000100ULL;
    ctx.r5 = bits(45.0f);
    ctx.r6 = bits(45.0f);
    ctx.r7 = bits(-560.0f);
    ctx.r16 = ctx.r4;
    ctx.f6.u32l = bits(75.0f);
    const auto original=ctx;
    // Neutral hooks must not access guest memory, including on a null RDRAM ptr.
    tooie_camera_hook_invert(nullptr,&ctx);
    tooie_camera_hook_request(nullptr,&ctx);
    tooie_camera_hook_transition_setup(nullptr,&ctx);
    tooie_camera_hook_request_complete(nullptr,&ctx);
    tooie_camera_hook_motion_enter(nullptr,&ctx);
    tooie_camera_hook_motion_speed(nullptr,&ctx);
    tooie_camera_hook_motion_acceleration(nullptr,&ctx);
    tooie_camera_hook_tick(nullptr,&ctx,0);
    tooie_camera_hook_motion_leave(nullptr,&ctx);
    CHECK(ctx.r5==original.r5 && ctx.r6==original.r6 &&
           ctx.r7==original.r7 && ctx.f6.u32l==original.f6.u32l);

    set_invert_horizontal(true);
    tooie_camera_hook_invert(nullptr,&ctx);
    CHECK(static_cast<uint32_t>(ctx.r5)==static_cast<uint32_t>(bits(45.0f)^0x80000000));
    set_invert_horizontal(false);
    ctx.r5 = bits(45.0f);

    alignas(8) std::array<uint8_t, 4096> memory{};
    uint8_t* rdram = memory.data();
    const auto cam=static_cast<gpr>(ctx.r4);
    const auto state=static_cast<gpr>(0xFFFFFFFF80000200ULL);
    MEM_W(0x60,cam)=static_cast<int32_t>(state);
    MEM_H(0x0C,state)=1;
    MEM_H(0x0E,state)=2;
    MEM_W(0x10,state)=bits(45.0f);

    set_rotation_speed(4.0f);
    CHECK(snapshot().rotation_speed == 4.0f);
    tooie_camera_hook_request(rdram,&ctx);
    tooie_camera_hook_transition_setup(rdram,&ctx);
    tooie_camera_hook_request_complete(rdram,&ctx);
    tooie_camera_hook_motion_enter(rdram,&ctx);
    ctx.r6 = bits(1000.0f); // the original motion controller loads this value
    tooie_camera_hook_motion_speed(rdram,&ctx);
    tooie_camera_hook_motion_acceleration(rdram,&ctx);
    CHECK(from(ctx.f6.u32l)==300.0f &&
           from(ctx.r6)==4000.0f && from(ctx.r7)==-2240.0f);
    tooie_camera_hook_motion_leave(rdram,&ctx);

    // A live change to neutral settings must stop modifying the next update.
    set_rotation_speed(1.0f);
    CHECK(snapshot().rotation_speed == 1.0f);
    ctx.f6.u32l=bits(75.0f);ctx.r6=bits(1000.0f);ctx.r7=bits(-560.0f);
    tooie_camera_hook_motion_enter(nullptr,&ctx);
    tooie_camera_hook_motion_speed(nullptr,&ctx);
    tooie_camera_hook_motion_acceleration(nullptr,&ctx);
    CHECK(from(ctx.f6.u32l)==75.0f && from(ctx.r6)==1000.0f && from(ctx.r7)==-560.0f);
    tooie_camera_hook_motion_leave(nullptr,&ctx);

    // Returning to 4x must *not* revive the old tagged transition.
    set_rotation_speed(4.0f);
    tooie_camera_hook_motion_enter(rdram,&ctx);
    tooie_camera_hook_motion_speed(rdram,&ctx);
    CHECK(from(ctx.f6.u32l)==75.0f);
    tooie_camera_hook_motion_leave(rdram,&ctx);
    puts("PASS: neutral path, inversion, manual scaling and live invalidation");
    return 0;
}
