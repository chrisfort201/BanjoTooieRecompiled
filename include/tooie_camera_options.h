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
uint32_t tooie_camera_manual_speed_bits(void);

// First-class N64Recomp TOML hooks (see banjotooie.us.toml).
void tooie_camera_hook_invert(uint8_t* rdram, recomp_context* ctx);
void tooie_camera_hook_rate(uint8_t* rdram, recomp_context* ctx);
void tooie_camera_hook_rate_complete(uint8_t* rdram, recomp_context* ctx);
void tooie_camera_hook_tick(uint8_t* rdram, recomp_context* ctx, int after);

#ifdef __cplusplus
}
#endif

#endif // TOOIE_CAMERA_OPTIONS_H
