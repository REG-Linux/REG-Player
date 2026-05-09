#include "gamepad.h"
#include <SDL3/SDL.h>
#include <unordered_map>
#include <unordered_set>

namespace gamepad {

static SDL_Gamepad* g_pad = nullptr;
static std::unordered_set<std::string> g_down;

void init() {
    int n = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&n);
    for (int i = 0; i < n; ++i) {
        if (SDL_IsGamepad(ids[i])) {
            g_pad = SDL_OpenGamepad(ids[i]);
            if (g_pad) break;
        }
    }
    if (ids) SDL_free(ids);
}

void shutdown() {
    if (g_pad) { SDL_CloseGamepad(g_pad); g_pad = nullptr; }
}

std::string keycode_to_logical(SDL_Keycode k) {
    switch (k) {
        case SDLK_UP:        return "up";
        case SDLK_DOWN:      return "down";
        case SDLK_LEFT:      return "left";
        case SDLK_RIGHT:     return "right";
        case SDLK_RETURN:    return "return";
        case SDLK_BACKSPACE: return "backspace";
        case SDLK_ESCAPE:    return "escape";
        case SDLK_SPACE:     return "space";
        case SDLK_PAGEUP:    return "pageup";
        case SDLK_PAGEDOWN:  return "pagedown";
        case SDLK_A:         return "a";
        case SDLK_B:         return "b";
        case SDLK_X:         return "x";
        case SDLK_Y:         return "y";
        case SDLK_L:         return "l";
        case SDLK_E:         return "e";
        default: return "";
    }
}

// Match xmplayer.gptk:
// a -> enter (return), b -> backspace, x -> x, y -> y
// l1 -> pageup, r1 -> pagedown, l2 -> l, r2 -> e
// start -> return, select -> space
// dpad up/down/left/right -> up/down/left/right
// guide -> esc
std::string gamepad_button_to_logical(SDL_GamepadButton b) {
    switch (b) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:        return "up";
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return "down";
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return "left";
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return "right";
        case SDL_GAMEPAD_BUTTON_SOUTH:          return "return";   // A button
        case SDL_GAMEPAD_BUTTON_EAST:           return "backspace"; // B
        case SDL_GAMEPAD_BUTTON_WEST:           return "x";        // X
        case SDL_GAMEPAD_BUTTON_NORTH:          return "y";        // Y
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return "pageup";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "pagedown";
        case SDL_GAMEPAD_BUTTON_START:          return "return";
        case SDL_GAMEPAD_BUTTON_BACK:           return "space";
        case SDL_GAMEPAD_BUTTON_GUIDE:          return "escape";
        default: return "";
    }
}

void on_event(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!g_pad && SDL_IsGamepad(e.gdevice.which)) {
                g_pad = SDL_OpenGamepad(e.gdevice.which);
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (g_pad && e.gdevice.which == SDL_GetGamepadID(g_pad)) {
                SDL_CloseGamepad(g_pad);
                g_pad = nullptr;
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
            std::string l = gamepad_button_to_logical((SDL_GamepadButton)e.gbutton.button);
            if (!l.empty()) g_down.insert(l);
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            std::string l = gamepad_button_to_logical((SDL_GamepadButton)e.gbutton.button);
            if (!l.empty()) g_down.erase(l);
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            // Treat L2/R2 trigger > 50% as button press for "l"/"e"
            const Sint16 thr = 16384;
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
                if (e.gaxis.value > thr) g_down.insert("l"); else g_down.erase("l");
            } else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
                if (e.gaxis.value > thr) g_down.insert("e"); else g_down.erase("e");
            }
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            std::string l = keycode_to_logical(e.key.key);
            if (!l.empty()) g_down.insert(l);
            break;
        }
        case SDL_EVENT_KEY_UP: {
            std::string l = keycode_to_logical(e.key.key);
            if (!l.empty()) g_down.erase(l);
            break;
        }
    }
}

bool is_logical_down(const std::string& logical) {
    return g_down.count(logical) > 0;
}

} // namespace gamepad
