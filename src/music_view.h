#pragma once
#include <string>

namespace music_view {

extern bool buttons_locked;

void init();
void on_music_opened();
void on_music_closed();
void register_user_input();
bool is_display_sleeping();
void update(float dt);
void draw();
bool keypressed(const std::string& key);

} // namespace music_view
