#ifndef TOOIE_CAMERA_SETTINGS_HPP
#define TOOIE_CAMERA_SETTINGS_HPP

namespace tooie::camera {
struct Settings {
    bool invert_horizontal = false;
    float rotation_speed = 1.0f;
    bool debug_trace = false;
};

// Used only to select defaults for the first launch without camera.json.
// RecompFrontend's camera.json is authoritative once the tab has been saved.
Settings legacy_ini_defaults();

// Called by RecompFrontend on its UI thread; the game thread reads atomics.
void set_invert_horizontal(bool enabled);
void set_rotation_speed(float multiplier);
void set_debug_trace(bool enabled);
Settings snapshot();

void register_config_tab();        // Before recompui::config::finalize().
void apply_loaded_config();        // After finalize(), including first launch.
} // namespace tooie::camera

#endif
