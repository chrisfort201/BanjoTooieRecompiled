// A single opt-in setting, managed by the same RecompFrontend configuration
// system as Camera. Saved as breegull.json in the application's config folder.
#include "tooie_breegull_settings.hpp"
#include "recompui/config.h"

#include <variant>

namespace tooie::breegull {
namespace {
constexpr const char* tab_id = "breegull";
constexpr const char* option_id = "modern_controls";
constexpr const char* invert_id = "invert_vertical_look";
constexpr const char* vertical_speed_id = "normal_vertical_speed";
void apply(const recomp::config::ConfigValueVariant& value) {
    if (const bool* selected = std::get_if<bool>(&value)) {
        set_modern_controls(*selected);
    }
}
void apply_invert(const recomp::config::ConfigValueVariant& value) {
    if (const bool* selected = std::get_if<bool>(&value)) {
        set_invert_vertical_look(*selected);
    }
}
void apply_vertical_speed(const recomp::config::ConfigValueVariant& value) {
    if (const double* multiplier = std::get_if<double>(&value)) {
        set_normal_vertical_speed(static_cast<float>(*multiplier));
    }
}
}
void register_config_tab() {
    auto& config = recompui::config::create_config_tab("Breegull Blaster", tab_id, false);
    config.add_bool_option(option_id, "Modern Breegull Blaster controls",
        "In Breegull Blaster, move/strafe with the N64 movement stick "
        "and look/aim with the configured C-direction controls, including "
        "vertical R-held aiming. Designed for dual-stick controller layouts. "
        "Original controls are used when off. "
        "Press F9 in-game to start/stop CSV input recording; see "
        "BREEGULL_INPUT_TRACE.md for the trace folder.", false);
    config.add_option_change_callback(option_id,
        [](auto current, auto, auto) { apply(current); });
    config.add_bool_option(invert_id, "Invert Vertical Look",
        "On preserves the game's original vertical look direction "
        "(stick down looks up). Off uses standard FPS vertical look. "
        "Only affects Modern Breegull Blaster controls.", true);
    config.add_option_disable_dependency(invert_id, option_id, false);
    config.add_option_change_callback(invert_id,
        [](auto current, auto, auto) { apply_invert(current); });

    config.add_number_option(vertical_speed_id,
        "Vertical Look Speed (Outside R-Aim)",
        "Changes only the ordinary Breegull Blaster C-up/down pitch speed. "
        "Does not affect horizontal turning or R-held precision aiming. "
        "1.0x means the original game's VERTICAL speed, not the same speed "
        "as horizontal looking. Modern controls must be enabled.",
        0.5, 3.0, 0.25, 2, false, 1.0);
    config.add_option_disable_dependency(vertical_speed_id, option_id, false);
    config.add_option_change_callback(vertical_speed_id,
        [](auto current, auto, auto) { apply_vertical_speed(current); });
}
void apply_loaded_config() {
    auto& config = recompui::config::get_config(tab_id);
    apply(config.get_option_value(option_id));
    apply_invert(config.get_option_value(invert_id));
    apply_vertical_speed(config.get_option_value(vertical_speed_id));
}
}
