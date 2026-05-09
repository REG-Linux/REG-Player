#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace gamepad {

void init();
void shutdown();
void on_event(const SDL_Event& e);

// returns logical key string for SDL_Keycode or "" if unmapped.
// Mirrors the xmplayer.gptk mapping (so xmb.lua keypressed strings work).
std::string keycode_to_logical(SDL_Keycode k);

// Translate gamepad button to logical key string.
std::string gamepad_button_to_logical(SDL_GamepadButton b);

bool is_logical_down(const std::string& logical);

} // namespace gamepad
