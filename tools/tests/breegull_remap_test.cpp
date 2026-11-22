#include "tooie_breegull_remap.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace tooie::breegull;

// Assertions must stay effective even in Release/NDEBUG builds.
static void require(bool ok, const char* description) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    require(is_blaster_state(0xCA), "first-person idle accepted");
    require(is_blaster_state(0xD1), "movement accepted");
    require(is_blaster_state(0xD8), "egg-firing state accepted");
    require(is_blaster_state(0xD9), "post-shot state accepted");
    require(!is_blaster_state(0xD0), "non-blaster state rejected");
    require(!is_blaster_state(0xD2), "neighbor state rejected");
    require(!is_blaster_state(0xD7), "pre-firing neighbor rejected");

    // Packed samples are passed atomically from the frontend's input thread
    // to the N64 controller-consumer thread.
    const LookInput values{0.25f, 0.9f, 0.6f, 0.1f};
    const LookInput received = unpack_look(pack_look(values));
    require(std::abs(received.left - values.left) < 0.00003f &&
            std::abs(received.right - values.right) < 0.00003f &&
            std::abs(received.up - values.up) < 0.00003f &&
            std::abs(received.down - values.down) < 0.00003f,
            "four binding strengths retain their values in packed sample");

    // Rebound axis: proportional analog strength, no raw SDL axis dependency.
    require(look_axis(0.0f, 1.0f) == 80, "full positive input");
    require(look_axis(1.0f, 0.0f) == -80, "full negative input");
    const int half = look_axis(0.0f, 0.5f);
    require(half > 0 && half < 80, "half-deflected rebound axis stays analog");
    require(look_axis(0.0f, 0.1f) == 0, "small axis noise removed");
    require(look_axis(0.8f, 0.8f) == 0, "conflicting mapped directions cancel");
    require(look_axis(0.2f, 0.8f) > 0, "different strengths subtract");

    // Ordinary left stick X -> strafe; logical C directions -> analog yaw.
    Pad initial{static_cast<uint16_t>(0x8000u | c_up | c_right), -60, -46};
    Pad translated = remap(initial, {.right=0.5f, .up=1.0f});
    require((translated.buttons & 0x8000u) != 0, "other game buttons preserved");
    require((translated.buttons & c_left) != 0 &&
            (translated.buttons & c_right) == 0, "left analog X strafes left");
    require(translated.stick_x > 0 && translated.stick_x < 80,
            "mapped analog yaw is proportional");
    require((translated.buttons & c_up) != 0,
            "ordinary C-up preserved by default");
    require(translated.stick_y == initial.stick_y,
            "forward/backward left stick Y preserved in normal mode");

    const Pad neutral = remap({0x0020u, 0, 31}, {});
    require(neutral.buttons == 0x0020u && neutral.stick_x == 0 &&
            neutral.stick_y == 31, "neutral look leaves other inputs unchanged");
    const Pad sideways = remap({0, 80, 0}, {});
    require((sideways.buttons & (c_left | c_right)) == c_right &&
            sideways.stick_x == 0, "sideways only strafes");
    const Pad diagonal = remap({0, 56, 56}, {});
    require((diagonal.buttons & c_right) != 0 && diagonal.stick_y == 56,
            "diagonal move keeps vertical motion");
    const Pad opposite = remap({c_right, 80, 0}, {.left=1.0f});
    require((opposite.buttons & c_right) != 0 && opposite.stick_x == -80,
            "rebound C-left turns left while left stick strafes right");
    require(remap({0,0,0}, {.right=1.0f}).stick_x == 80,
            "mapped look works without physical right stick");

    // In normal mode, inversion acts on the C-up/down buttons the game reads.
    const Pad normal = remap({c_up, 0, 23}, {.up=1.0f}, true);
    require((normal.buttons & c_up) != 0 && normal.stick_y == 23,
            "default original inverted look preserved");
    const Pad standard = remap({c_up, 0, 23}, {.up=1.0f}, false);
    require((standard.buttons & c_down) != 0 &&
            (standard.buttons & c_up) == 0 && standard.stick_y == 23,
            "standard look swaps C-up for C-down without movement change");
    require((remap({c_down, 0, 0}, {.down=1.0f}, false).buttons & c_up) != 0,
            "standard look swaps C-down for C-up");

    // R-held aim uses the same mapped look directions for both axes, while
    // keeping R itself and the game's existing movement restrictions intact.
    const Pad held{static_cast<uint16_t>(r_aim | c_up | c_left), 0, -75};
    const Pad aim_inverted = remap(held, {.left=1.0f,.up=1.0f}, true);
    require(aim_inverted.stick_x == -80 && aim_inverted.stick_y == 80,
            "R-held aiming maps both look axes");
    require((aim_inverted.buttons & r_aim) != 0, "R preserved");
    const Pad aim_normal = remap(held, {.up=1.0f}, false);
    require(aim_normal.stick_y == -80,
            "R-held vertical inversion changes only pitch sign");
    require(remap({r_aim, 0, 80}, {}, true).stick_y == 0,
            "left-stick Y no longer independently controls R-held pitch");
    require(remap({static_cast<uint16_t>(r_aim|c_up),0,0}, {.up=1.0f}, true).stick_y == 80,
            "keyboard/controller digital C-up gives full pitch");
    const int analog_pitch = remap({r_aim, 0, 0}, {.down=0.5f}, true).stick_y;
    require(analog_pitch < 0 && analog_pitch > -80,
            "arbitrary analog vertical binding preserves proportional pitch");
    require(remap({r_aim,0,0}, {.up=1.0f,.down=1.0f}, true).stick_y == 0,
            "opposing vertical look inputs cancel");

    // Firing changes player state to D8; never discard the game action while
    // preserving strafing plus an independent mapped turn request.
    const Pad firing = remap({static_cast<uint16_t>(0x2000u|c_right), 81,0},
                             {.right=1.0f});
    require((firing.buttons & 0x2000u) != 0 &&
            (firing.buttons & c_right) != 0 && firing.stick_x == 80,
            "egg firing keeps strafe, yaw and firing button");

    std::puts("PASS: Breegull mapped-profile analog, R aim and inversion");
}
