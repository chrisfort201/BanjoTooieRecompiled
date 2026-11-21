// Camera controls are ordinary RecompFrontend configuration, persisted in the
// application's config directory. Callbacks update thread-safe live settings.
#include "tooie_camera_settings.hpp"

#include "recompui/config.h"

#include <variant>

namespace tooie::camera {
namespace {
constexpr const char* tab_id = "camera";
constexpr const char* invert_id = "invert_horizontal";
constexpr const char* speed_id = "rotation_speed";
constexpr const char* trace_id = "debug_trace";

void apply_inversion(const recomp::config::ConfigValueVariant& value) {
    if (const bool* enabled = std::get_if<bool>(&value)) {
        set_invert_horizontal(*enabled);
    }
}
void apply_speed(const recomp::config::ConfigValueVariant& value) {
    if (const double* multiplier = std::get_if<double>(&value)) {
        set_rotation_speed(static_cast<float>(*multiplier));
    }
}
void apply_trace(const recomp::config::ConfigValueVariant& value) {
    if (const bool* enabled = std::get_if<bool>(&value)) {
        set_debug_trace(*enabled);
    }
}
} // namespace

void register_config_tab() {
    // Only for the initial migration: camera.json takes precedence on every
    // later launch. New installations have the original neutral defaults.
    const Settings initial = legacy_ini_defaults();
    auto& config = recompui::config::create_config_tab("Camera", tab_id, false);
    config.add_bool_option(invert_id, "Invert horizontal camera",
        "Reverse C-left/C-right during manual third-person camera turns. "
        "Does not remap C-buttons in first-person mode.", initial.invert_horizontal);
    config.add_number_option(speed_id, "Manual camera rotation speed",
        "Multiplier for manual third-person yaw; 1.0x is original N64 speed.",
        1.0, 4.0, 0.25, 2, false, initial.rotation_speed);
    config.add_bool_option(trace_id, "Camera diagnostic trace",
        "Log camera transition measurements to camera-debug.csv (for debugging).",
        initial.debug_trace);

    config.add_option_change_callback(invert_id,
        [](auto current, auto, auto) { apply_inversion(current); });
    config.add_option_change_callback(speed_id,
        [](auto current, auto, auto) { apply_speed(current); });
    config.add_option_change_callback(trace_id,
        [](auto current, auto, auto) { apply_trace(current); });
}

void apply_loaded_config() {
    // finalize() loads the persisted JSON. Initialize the game-thread atomics
    // explicitly too, without depending on callback behavior during Load.
    auto& config = recompui::config::get_config(tab_id);
    apply_inversion(config.get_option_value(invert_id));
    apply_speed(config.get_option_value(speed_id));
    apply_trace(config.get_option_value(trace_id));
}
} // namespace tooie::camera
