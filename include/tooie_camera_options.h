#ifndef TOOIE_CAMERA_OPTIONS_H
#define TOOIE_CAMERA_OPTIONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Accept/return the original MIPS float bit pattern; used only by the
// third-person manual yaw path, never by the controller-input layer.
uint32_t tooie_camera_manual_yaw_bits(uint32_t original_bits);
uint32_t tooie_camera_manual_speed_bits(void);

#ifdef __cplusplus
}
#endif

#endif // TOOIE_CAMERA_OPTIONS_H
