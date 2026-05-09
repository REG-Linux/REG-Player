#include "image_view.h"
#include "assets.h"
#include "common.h"
#include "gamepad.h"
#include "image_viewer.h"
#include "render.h"
#include "ui.h"
#include <SDL3/SDL.h>
#include <cstdio>

namespace image_view {

static const float zoom_speed = 1.2f;

void draw() {
    if (!image_viewer::s.active) return;
    int w = g_app.screen_w, h = g_app.screen_h;
    render::set_color(Color{0, 0, 0, image_viewer::s.fade_alpha});
    render::rect_fill(0, 0, w, h);

    if (image_viewer::s.current_image) {
        SDL_SetTextureColorModFloat(image_viewer::s.current_image, 1, 1, 1);
        SDL_SetTextureAlphaModFloat(image_viewer::s.current_image, image_viewer::s.fade_alpha);
        render::draw_texture_scaled(image_viewer::s.current_image, image_viewer::s.pan_x, image_viewer::s.pan_y, image_viewer::s.zoom);
    } else {
        ui::draw_text_aligned("No image loaded", 0, h / 2, w, assets::fonts.small, Color{1, 1, 1, image_viewer::s.fade_alpha}, "center");
    }

    if (!image_viewer::s.playlist.empty()) {
        const auto& item = image_viewer::s.playlist[image_viewer::s.current_index];
        render::set_color(Color{0, 0, 0, 0.4f * image_viewer::s.fade_alpha});
        render::rect_fill(0, h - 50, w, 50);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "[%d/%zu] %s",
            image_viewer::s.current_index + 1, image_viewer::s.playlist.size(), item.name.c_str());
        ui::draw_text_aligned(buf, 20, h - 35, w - 40, assets::fonts.xs, Color{1, 1, 1, 0.8f * image_viewer::s.fade_alpha}, "left");
    }
}

bool keypressed(const std::string& key) {
    if (!image_viewer::s.active) return false;
    if (key == "space") { image_viewer::close(); return true; }
    if (key == "a" || key == "return") { image_viewer::reset_view(); return true; }
    if (key == "x") { image_viewer::s.zoom *= zoom_speed; return true; }
    if (key == "b" || key == "backspace") { image_viewer::s.zoom /= zoom_speed; return true; }
    if (!gamepad::is_logical_down("y")) {
        if (key == "right") { image_viewer::next_image(); return true; }
        if (key == "left")  { image_viewer::prev_image(); return true; }
    }
    return false;
}

} // namespace image_view
