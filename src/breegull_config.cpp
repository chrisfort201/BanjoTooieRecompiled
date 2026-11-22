// A single opt-in setting, managed by the same RecompFrontend configuration
// system as Camera. Saved as breegull.json in the application's config folder.
#include "tooie_breegull_settings.hpp"
#include "recompui/config.h"

#include <variant>

namespace tooie::breegull {
namespace {
constexpr const char* tab_id = "breegull";
constexpr const char* option_id = "modern_controls";
void apply(const recomp::config::ConfigValueVariant& value) {
    if (const bool* selected = std::get_if<bool>(&value)) {
        set_modern_controls(*selected);
    }
}
}
void register_config_tab() {
    auto& config = recompui::config::create_config_tab("Breegull Blaster", tab_id, false);
    config.add_bool_option(option_id, "Modern Breegull Blaster controls",
        "In Breegull Blaster, move/strafe with the left stick and turn/aim "
        "with the right stick. Original controls are used when off. "
        "Press F9 in-game to start/stop CSV input recording; see "
        "BREEGULL_INPUT_TRACE.md for the trace folder.", false);
    config.add_option_change_callback(option_id,
        [](auto current, auto, auto) { apply(current); });
}
void apply_loaded_config() {
    apply(recompui::config::get_config(tab_id).get_option_value(option_id));
}
}
