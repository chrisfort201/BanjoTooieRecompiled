#include "tooie_breegull_remap.hpp"

#include <cstdio>
#include <cstdlib>

using tooie::breegull::Pad;
using tooie::breegull::remap;

// These checks must remain effective with NDEBUG / Release builds.
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    // C-Up and C-Down are pitch controls and must never be turned into strafe.
    constexpr uint16_t c_up = 0x0008u;
    constexpr uint16_t c_down = 0x0004u;
    constexpr uint16_t c_left = 0x0002u;
    constexpr uint16_t c_right = 0x0001u;
    static_assert(tooie::breegull::c_left == c_left);
    static_assert(tooie::breegull::c_right == c_right);
    require(tooie::breegull::is_blaster_state(0xCA), "first-person idle accepted");
    require(tooie::breegull::is_blaster_state(0xD1), "first-person movement accepted");
    require(tooie::breegull::is_blaster_state(0xD8), "egg-firing state accepted");
    require(tooie::breegull::is_blaster_state(0xD9), "adjacent blaster state retained");
    require(!tooie::breegull::is_blaster_state(0xD7), "unobserved state rejected");
    require(!tooie::breegull::is_blaster_state(0xD0), "other state rejected");
    require(!tooie::breegull::is_blaster_state(0xD2), "neighboring state rejected");

    Pad initial{static_cast<uint16_t>(0x8000u | c_up | c_down), -60, -46};
    Pad left = remap(initial, 25000);
    require((left.buttons & 0x8000u) != 0, "fire button preserved");
    require((left.buttons & c_left) != 0, "left stick X strafes left");
    require((left.buttons & c_right) == 0, "right strafe remains off");
    require((left.buttons & (c_up | c_down)) == (c_up | c_down),
            "both pitch buttons preserved");
    require(left.stick_x > 0, "right stick X turns right");
    require(left.stick_y == initial.stick_y, "forward/back preserved");

    Pad center = remap({static_cast<uint16_t>(c_left | c_right | 0x0020u), 0, 31}, 0);
    require(center.buttons == 0x0020u && center.stick_x == 0 && center.stick_y == 31,
            "neutral axes cancel strafe and yaw without losing other buttons");

    Pad sideways = remap({0, 80, 0}, 0);
    require((sideways.buttons & (c_left | c_right)) == c_right &&
                sideways.stick_x == 0 && sideways.stick_y == 0,
            "pure sideways left stick must strafe and never turn");
    // Z (0x2000) is the observed egg-firing input. Retain it while converting
    // left-stick X to strafing; animation state validation is tested above.
    Pad shooting = remap({0x2000u, -80, 0}, 0);
    require(shooting.buttons == static_cast<uint16_t>(0x2000u | c_left) &&
                shooting.stick_x == 0,
            "firing while strafing preserves fire and does not turn");
    Pad diagonal = remap({0, 56, 56}, 0);
    require((diagonal.buttons & (c_left | c_right)) == c_right &&
                diagonal.stick_x == 0 && diagonal.stick_y == 56,
            "diagonal left stick must strafe plus walk without turning");

    Pad right = remap({0, 80, 0}, -32768);
    require((right.buttons & c_right) != 0 && right.stick_x == -80,
            "right strafe and full left yaw");

    Pad deadzone = remap({c_right, 21, 70}, 3000);
    require((deadzone.buttons & (c_left | c_right)) == 0 && deadzone.stick_x == 0,
            "both deadzones work");

    std::puts("PASS: Breegull N64 input translation (Release assertions active)");
}
