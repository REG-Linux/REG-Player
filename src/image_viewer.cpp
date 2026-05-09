#include "image_viewer.h"
#include "common.h"
#include "gamepad.h"
#include "utils.h"
#include <SDL3_image/SDL_image.h>
#include <algorithm>

namespace image_viewer {

State s;
static const float pan_speed = 400.0f;

void init() {}

void load_image(const std::string& path) {
    if (s.current_image) { SDL_DestroyTexture(s.current_image); s.current_image = nullptr; }
    s.current_image = IMG_LoadTexture(g_app.renderer, path.c_str());
    if (s.current_image) {
        SDL_SetTextureScaleMode(s.current_image, SDL_SCALEMODE_LINEAR);
        reset_view();
    }
}

void reset_view() {
    if (!s.current_image) return;
    int w = g_app.screen_w, h = g_app.screen_h;
    float iw, ih;
    SDL_GetTextureSize(s.current_image, &iw, &ih);
    float sw = w / iw, sh = h / ih;
    s.zoom = std::min(sw, sh);
    s.pan_x = w * 0.5f;
    s.pan_y = h * 0.5f;
}

void open(const std::string& path, const std::vector<browser::File>& files_list) {
    s.active = true;
    s.fade_alpha = 0;
    s.current_path = path;
    s.playlist.clear();
    static const char* exts[] = {".jpg", ".jpeg", ".png", ".bmp", ".tga", nullptr};
    for (auto& f : files_list) {
        if (f.type == "file") {
            std::string ext = utils::get_extension(f.path);
            for (int i = 0; exts[i]; ++i) if (ext == exts[i]) { s.playlist.push_back(f); break; }
        }
    }
    s.current_index = 0;
    for (size_t i = 0; i < s.playlist.size(); ++i) {
        if (s.playlist[i].path == path) { s.current_index = (int)i; break; }
    }
    load_image(path);
}

void next_image() {
    if (s.playlist.empty()) return;
    s.current_index = (s.current_index + 1) % (int)s.playlist.size();
    load_image(s.playlist[s.current_index].path);
}
void prev_image() {
    if (s.playlist.empty()) return;
    s.current_index = (s.current_index - 1 + (int)s.playlist.size()) % (int)s.playlist.size();
    load_image(s.playlist[s.current_index].path);
}

void close() {
    s.active = false;
    if (s.current_image) { SDL_DestroyTexture(s.current_image); s.current_image = nullptr; }
}

void update(float dt) {
    if (!s.active) return;
    if (s.fade_alpha < 1) s.fade_alpha = std::min(1.0f, s.fade_alpha + dt * 4);
    if (gamepad::is_logical_down("y")) {
        if (gamepad::is_logical_down("up"))    s.pan_y += pan_speed * dt;
        if (gamepad::is_logical_down("down"))  s.pan_y -= pan_speed * dt;
        if (gamepad::is_logical_down("left"))  s.pan_x += pan_speed * dt;
        if (gamepad::is_logical_down("right")) s.pan_x -= pan_speed * dt;
    }
}

} // namespace image_viewer
