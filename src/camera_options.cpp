// Optional third-person camera adjustments. This intentionally does not change
// C-button state: the calls below come only from the game's manual camera turn
// routine (see CAMERA_PATCH.md for the verified call chain).
#include "tooie_camera_options.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {
struct CameraOptions {
    bool invert_horizontal = false;
    float rotation_speed = 1.0f;
};

std::string trim(std::string text) {
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

CameraOptions read_options() {
    CameraOptions options;
    std::ifstream file("camera.ini"); // Same working directory as assets/.
    std::string line;
    while (std::getline(file, line)) {
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) line.resize(comment);
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = lowercase(trim(line.substr(0, eq)));
        const std::string value = lowercase(trim(line.substr(eq + 1)));
        if (key == "invert_horizontal") {
            if (value == "true" || value == "on" || value == "yes" || value == "1") {
                options.invert_horizontal = true;
            } else if (value == "false" || value == "off" || value == "no" || value == "0") {
                options.invert_horizontal = false;
            }
        } else if (key == "rotation_speed") {
            char* end = nullptr;
            const float parsed = std::strtof(value.c_str(), &end);
            if (end != value.c_str() && *end == '\0' && std::isfinite(parsed) &&
                parsed >= 1.0f && parsed <= 4.0f) {
                options.rotation_speed = parsed;
            }
        }
    }
    return options;
}

const CameraOptions& options() {
    // Thread-safe lazy load; settings are applied on next launch, not per frame.
    static const CameraOptions value = read_options();
    return value;
}
} // namespace

extern "C" uint32_t tooie_camera_manual_yaw_bits(uint32_t original_bits) {
    // Only the two manual-turn branches pass through here (+/- 45 degrees).
    return options().invert_horizontal ? (original_bits ^ 0x80000000u) : original_bits;
}

extern "C" uint32_t tooie_camera_manual_speed_bits(void) {
    // Original camera transition is given 45.0f as its rotation-rate limit.
    // Increasing this denominator shortens the interpolation time without
    // changing the +/-45 degree target angle or frame-rate-dependent logic.
    const float speed = options().rotation_speed;
    if (speed == 1.0f) return 0x42340000u; // exact original 45.0f bits
    const float scaled = 45.0f * speed;
    uint32_t bits = 0;
    static_assert(sizeof bits == sizeof scaled);
    std::memcpy(&bits, &scaled, sizeof bits);
    return bits;
}
