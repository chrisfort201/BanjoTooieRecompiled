#ifndef TOOIE_CAMERA_OPTIONS_H
#define TOOIE_CAMERA_OPTIONS_H

#include <stdint.h>
#include "recomp.h"

#ifdef __cplusplus
extern "C" {
#endif

// Accept/return the original MIPS float bit pattern; used only by the
// third-person manual yaw path, never by the controller-input layer.
uint32_t tooie_camera_manual_yaw_bits(uint32_t original_bits);

// First-class N64Recomp TOML hooks (see banjotooie.us.toml).
void tooie_camera_hook_invert(uint8_t* rdram, recomp_context* ctx);
void tooie_camera_hook_request(uint8_t* rdram, recomp_context* ctx);
void tooie_camera_hook_request_complete(uint8_t* rdram, recomp_context* ctx);
// Invalidates the manual tag if another transition takes over that camera.
void tooie_camera_hook_transition_setup(uint8_t* rdram, recomp_context* ctx);
// Bracket only the type-1 motion controller call, preserving scripted motion.
void tooie_camera_hook_motion_enter(uint8_t* rdram, recomp_context* ctx);
void tooie_camera_hook_motion_leave(uint8_t* rdram, recomp_context* ctx);
void tooie_camera_hook_motion_speed(uint8_t* rdram, recomp_context* ctx);
void tooie_camera_hook_motion_acceleration(uint8_t* rdram, recomp_context* ctx);
void tooie_camera_hook_tick(uint8_t* rdram, recomp_context* ctx, int after);

#ifdef __cplusplus
}
#endif

#endif // TOOIE_CAMERA_OPTIONS_H
