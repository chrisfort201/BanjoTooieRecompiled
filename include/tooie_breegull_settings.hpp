#ifndef TOOIE_BREEGULL_SETTINGS_HPP
#define TOOIE_BREEGULL_SETTINGS_HPP

namespace tooie::breegull {
void set_modern_controls(bool enabled);
bool modern_controls_enabled();
void set_invert_vertical_look(bool inverted);
void sample_mapped_look(); // UI/SDL event thread, after handle_events()
void register_config_tab();
void apply_loaded_config();
}

#endif
