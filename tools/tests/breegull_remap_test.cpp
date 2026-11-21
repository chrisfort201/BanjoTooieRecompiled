#include "tooie_breegull_remap.hpp"

#include <cassert>
#include <cstdio>
using tooie::breegull::Pad;
using tooie::breegull::remap;
int main() {
    // Moving and looking are independent; vertical movement is untouched.
    Pad initial{0x8000u | 0x0008u, -60, -46};
    Pad left = remap(initial, 25000);
    assert((left.buttons & 0x8000u) != 0); // Fire/other action remains.
    assert((left.buttons & 0x0008u) != 0); // Left-stick X -> strafe left.
    assert((left.buttons & 0x0004u) == 0);
    assert(left.stick_x > 0);              // Right-stick X -> turn right.
    assert(left.stick_y == initial.stick_y);
    Pad center = remap({0x000Cu | 0x0020u, 0, 31}, 0);
    assert(center.buttons == 0x0020u && center.stick_x == 0 && center.stick_y == 31);
    Pad right = remap({0, 80, 0}, -32768);
    assert((right.buttons & 0x0004u) != 0 && right.stick_x == -80);
    Pad deadzone = remap({0x0004u, 21, 70}, 3000);
    assert((deadzone.buttons & 0x000Cu) == 0 && deadzone.stick_x == 0);
    std::puts("PASS: Breegull N64 input translation");
}
