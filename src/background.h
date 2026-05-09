#pragma once
#include <string>

namespace music_player { struct State; }

namespace background {

void init();
void update(float dt, bool is_paused);
void draw_with_music_state(bool music_active, bool music_playing, bool music_paused, const std::string& visualizer_mode, const float* pcm, int pcm_count, int pcm_pos);

void set_custom_bg(bool enabled);
void set_custom_bg_path(const std::string& path);
void set_wallpaper_blur(bool enabled);
void set_wallpaper_tint(bool enabled);
void set_wallpaper_brightness(int mode_index);
bool has_custom_wallpaper();

} // namespace background
