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

    Pad right = remap({0, 80, 0}, -32768);
    require((right.buttons & c_right) != 0 && right.stick_x == -80,
            "right strafe and full left yaw");

    Pad deadzone = remap({c_right, 21, 70}, 3000);
    require((deadzone.buttons & (c_left | c_right)) == 0 && deadzone.stick_x == 0,
            "both deadzones work");

    std::puts("PASS: Breegull N64 input translation (Release assertions active)");
}
