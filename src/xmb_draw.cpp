// Port of xmb_draw.lua. Read-only renderer; nav state owned by xmb.cpp.
#include "xmb_draw.h"
#include "assets.h"
#include "browser.h"
#include "categories.h"
#include "common.h"
#include "indexing.h"
#include "render.h"
#include "settings_view.h"
#include "theme.h"
#include "ui.h"
#include "utils.h"
#include "video_manager.h"
#include "xmb.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace xmb_draw {

// Thumbnail texture cache (keyed on path) with LRU eviction.
static std::unordered_map<std::string, SDL_Texture*> g_thumbs;
static std::vector<std::string> g_thumb_order; // LRU: front = most recent
static const size_t THUMB_CACHE_MAX = 64;

static SDL_Texture* load_thumb(const std::string& path) {
    if (path.empty()) return nullptr;
    auto it = g_thumbs.find(path);
    if (it != g_thumbs.end()) {
        // Promote to most recent.
        auto pos = std::find(g_thumb_order.begin(), g_thumb_order.end(), path);
        if (pos != g_thumb_order.end() && pos != g_thumb_order.begin()) {
            g_thumb_order.erase(pos);
            g_thumb_order.insert(g_thumb_order.begin(), path);
        }
        return it->second;
    }
    SDL_Texture* t = IMG_LoadTexture(g_app.renderer, path.c_str());
    if (t) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    g_thumbs[path] = t;
    g_thumb_order.insert(g_thumb_order.begin(), path);
    while (g_thumb_order.size() > THUMB_CACHE_MAX) {
        const std::string& victim = g_thumb_order.back();
        auto vit = g_thumbs.find(victim);
        if (vit != g_thumbs.end()) {
            if (vit->second) SDL_DestroyTexture(vit->second);
            g_thumbs.erase(vit);
        }
        g_thumb_order.pop_back();
    }
    return t;
}

void draw() {
    int sw = g_app.screen_w, sh = g_app.screen_h;
    auto& cats = categories::list();
    float cat_base_x = sw * 0.25f;
    float cat_y = sh * 0.25f;

    // Category bar
    render::push_translate(cat_base_x + xmb::category_scroll_x, cat_y);
    for (size_t i = 0; i < cats.size(); ++i) {
        float x = (float)i * (theme::icon_size + theme::icon_spacing);
        bool focused = ((int)i + 1 == xmb::current_category_idx);
        SDL_Texture* img = assets::get_image("cat_" + cats[i].id);
        if (!img) continue;
        float iw, ih;
        SDL_GetTextureSize(img, &iw, &ih);
        float base_scale = theme::icon_size / iw;
        float scale = focused ? base_scale * 1.1f : base_scale * 0.7f;
        float alpha = focused ? 0.8f : 0.4f;
        if (focused) {
            ui::draw_glow_icon(img, x, 0, theme::icon_size * 1.1f, theme::text, alpha, &theme::text);
            Color tc{theme::text.r, theme::text.g, theme::text.b, alpha};
            ui::draw_glow_text(cats[i].name, x - 100, theme::icon_size / 2 + 12, assets::fonts.main, tc, nullptr, 200, "center");
        } else {
            SDL_SetTextureColorModFloat(img, theme::text.r, theme::text.g, theme::text.b);
            SDL_SetTextureAlphaModFloat(img, alpha);
            render::draw_texture_scaled(img, x, 0, scale, 0.5f, 0.5f);
        }
    }
    render::pop_translate();

    // Submenu back arrow
    if (xmb::in_submenu()) {
        float arrow_x = cat_base_x - 90;
        float arrow_y = cat_y + theme::icon_size + 87;
        double t = SDL_GetTicksNS() / 1.0e9;
        float pulse = 0.5f + 0.3f * std::sin(t * 3);
        SDL_Vertex v[3];
        SDL_FColor c{theme::text.r, theme::text.g, theme::text.b, pulse};
        v[0] = {{arrow_x + 12, arrow_y},        c, {0, 0}};
        v[1] = {{arrow_x + 24, arrow_y - 10},   c, {0, 0}};
        v[2] = {{arrow_x + 24, arrow_y + 10},   c, {0, 0}};
        SDL_RenderGeometry(g_app.renderer, nullptr, v, 3, nullptr, 0);
    }

    // Item list
    float list_x = cat_base_x + 32;
    float list_base_y = cat_y + theme::icon_size + 75;
    float fade_top = cat_y + theme::icon_size / 2.0f + 50;
    float fade_range = 100;

    render::set_clip(0, (int)(fade_top - 20), sw, sh - (int)(fade_top - 20));
    render::push_translate(list_x + xmb::list_slide_x, list_base_y + xmb::item_scroll_y);

    int item_h = 75;
    int first = std::max(1, (int)(-xmb::item_scroll_y / item_h) - 2);
    int last  = std::min((int)browser::files.size(), first + (int)std::ceil(sh / (float)item_h) + 4);

    auto& cats_ = categories::list();
    std::string cat_id = (xmb::current_category_idx >= 1 && xmb::current_category_idx <= (int)cats_.size())
        ? cats_[xmb::current_category_idx - 1].id : "";

    for (int i = first; i <= last; ++i) {
        const auto& it = browser::files[i - 1];
        float y = (float)(i - 1) * item_h;
        float screen_y = list_base_y + xmb::item_scroll_y + y;

        float item_alpha = xmb::list_slide_alpha;
        if (screen_y < list_base_y) {
            float dist = list_base_y - screen_y;
            item_alpha = std::max(0.0f, xmb::list_slide_alpha * (1.0f - (dist / fade_range)));
        }
        bool focused = (i == xmb::current_item_idx);
        float base_alpha = focused ? 0.8f : 0.5f;
        float final_alpha = base_alpha * item_alpha;
        if (final_alpha <= 0) continue;

        // Resolve icon + thumb
        SDL_Texture* icon = assets::get_image("folder");
        SDL_Texture* thumb = nullptr;

        if (it.type == "file" && (cat_id == "photo" || it.icon == "photo")) {
            icon = assets::get_image("photo");
            std::string thumb_path;
            {
                std::lock_guard<std::mutex> l(indexing::data_mutex);
                auto pit = indexing::data.photos.find(it.path);
                if (pit != indexing::data.photos.end() && !pit->second.thumb_path.empty())
                    thumb_path = pit->second.thumb_path;
            }
            if (thumb_path.empty()) thumb_path = it.path;
            thumb = load_thumb(thumb_path);
        } else if (!it.icon.empty() && assets::get_image(it.icon)) {
            icon = assets::get_image(it.icon);
        } else if (it.type == "directory") {
            icon = assets::get_image("folder");
        } else if (it.type == "album") {
            icon = assets::get_image("album");
            if (it.data) {
                auto* a = (indexing::AlbumEntry*)it.data;
                if (!a->thumb_path.empty()) thumb = load_thumb(a->thumb_path);
            }
        } else if (it.type == "artist") {
            icon = assets::get_image("artist");
        } else if (it.type == "file") {
            if (cat_id == "video") icon = assets::get_image("file_video");
            else if (cat_id == "music") {
                icon = (xmb::view_type == "album_tracks" || xmb::view_type == "artist_tracks")
                    ? assets::get_image("track") : assets::get_image("file_music");
            } else icon = assets::get_image("file");
        }

        if (focused) {
            ui::draw_glow_icon(icon, -36, y + 14, 48, theme::text, final_alpha, &theme::text, thumb);
        } else {
            ui::draw_icon(icon, -36, y + 14, 48, theme::text, final_alpha, thumb);
        }

        if (it.type == "file" && cat_id == "video" && video_manager::is_watched(it.path)) {
            ui::draw_icon(assets::get_image("eye"), -52, y, 36, Color{1, 1, 1, 1}, 1.0f);
        }

        if (focused) {
            float ty = it.description.empty() ? y : y - 8;
            Color tc{theme::text.r, theme::text.g, theme::text.b, final_alpha};
            ui::draw_marquee(xmb::item_marquee, it.name, 0, ty, assets::fonts.main, tc,
                             list_x + xmb::list_slide_x, screen_y + (it.description.empty() ? 0 : -8));
            if (!it.description.empty()) {
                float desc_y = y + 24;
                float line_w = sw * 0.7f;
                Color lc{theme::text.r, theme::text.g, theme::text.b, final_alpha * 0.3f};
                render::set_color(lc);
                render::line(0, desc_y, line_w, desc_y);
                Color dc{theme::text.r, theme::text.g, theme::text.b, final_alpha * 0.8f};
                ui::draw_text(it.description, 0, desc_y + 5, assets::fonts.xs, dc);
            }
        } else {
            Color tc{theme::text.r, theme::text.g, theme::text.b, final_alpha};
            ui::draw_text(it.display_name.empty() ? it.name : it.display_name, 0, y + 4, assets::fonts.small, tc);
        }
    }

    render::pop_translate();
    render::clear_clip();

    // Settings choice popup
    if (settings_view::active || settings_view::alpha > 0) {
        if (xmb::current_item_idx >= 1 && xmb::current_item_idx <= (int)browser::files.size()) {
            int sidx = browser::files[xmb::current_item_idx - 1].setting_idx;
            if (sidx >= 0) settings_view::draw_popup(sidx);
        }
    }
    if (settings_view::picker_active /* draw alpha tracked inside */) {
        settings_view::draw_folder_picker();
    }

    // Keyboard help overlay
    if (xmb::help_overlay_active || xmb::help_overlay_alpha > 0) {
        float a = xmb::help_overlay_alpha;
        float panel_w = sw * 0.55f;
        float panel_h = sh * 0.7f;
        float px = (sw - panel_w) * 0.5f;
        float py = (sh - panel_h) * 0.5f;
        render::set_color(Color{0.02f, 0.02f, 0.05f, 0.94f * a});
        render::rect_fill_rounded(px, py, panel_w, panel_h, 16);
        render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.4f * a});
        render::rect_line_rounded(px, py, panel_w, panel_h, 16, 2);
        Color tc{theme::text.r, theme::text.g, theme::text.b, a};
        ui::draw_glow_text("Controls", px + 30, py + 24, assets::fonts.title, tc, &theme::accent);
        struct Row { const char* btn; const char* desc; };
        static const Row rows[] = {
            {"D-Pad",    "Navigate"},
            {"A",        "Select / Confirm"},
            {"B",        "Back / Cancel"},
            {"X",        "Context Menu / Options / This Help"},
            {"Y",        "(Photo) Hold to pan / (Music+Right) Lock"},
            {"L1 / R1",  "Video: Prev / Next"},
            {"L2 / R2",  "Video: Subtitles toggle / next sub"},
            {"Start",    "Video: Play / Pause"},
            {"Select",   "Video / Photo: Return to XMB"},
            {nullptr, nullptr}
        };
        float row_y = py + 90;
        for (int i = 0; rows[i].btn; ++i) {
            ui::draw_text(rows[i].btn,  px + 40,  row_y, assets::fonts.small, Color{theme::accent.r, theme::accent.g, theme::accent.b, a});
            ui::draw_text(rows[i].desc, px + 240, row_y, assets::fonts.small, Color{theme::text.r,   theme::text.g,   theme::text.b,   a});
            row_y += 44;
        }
        ui::draw_text("Press B to close", px + 40, py + panel_h - 50, assets::fonts.xs, Color{1, 1, 1, 0.6f * a});
    }

    // Context menu popup
    if (xmb::context_menu.active || xmb::context_menu.alpha > 0) {
        float alpha = xmb::context_menu.alpha;
        float panel_w = 300;
        float panel_h = 140 + ((float)xmb::context_menu.items.size() * 50);
        float panel_x = sw - panel_w - 40;
        float panel_y = sh * 0.22f;

        render::set_color(Color{0.05f, 0.05f, 0.08f, 0.94f * alpha});
        render::rect_fill_rounded(panel_x, panel_y, panel_w, panel_h, 16);

        render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.35f * alpha});
        render::rect_line_rounded(panel_x, panel_y, panel_w, panel_h, 16, 2);

        Color tc{theme::text.r, theme::text.g, theme::text.b, alpha};
        ui::draw_glow_text(xmb::context_menu.title.empty() ? "Options" : xmb::context_menu.title,
                           panel_x + 24, panel_y + 20, assets::fonts.main, tc, nullptr);

        render::set_color(Color{theme::text.r, theme::text.g, theme::text.b, 0.22f * alpha});
        render::rect_fill(panel_x + 22, panel_y + 64, panel_w - 44, 1);

        for (size_t i = 0; i < xmb::context_menu.items.size(); ++i) {
            float row_y = panel_y + 82 + (float)i * 50;
            bool focused = ((int)i + 1 == xmb::context_menu.selected_idx);
            if (focused) {
                render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.28f * alpha});
                render::rect_fill_rounded(panel_x + 18, row_y - 6, panel_w - 36, 40, 10);
            }
            const auto& opt = xmb::context_menu.items[i];
            if (focused) {
                ui::draw_glow_text(opt.label, panel_x + 32, row_y, assets::fonts.small, tc, nullptr);
            } else {
                Color uc{theme::text.r, theme::text.g, theme::text.b, 0.75f * alpha};
                ui::draw_text(opt.label, panel_x + 32, row_y, assets::fonts.small, uc);
            }
        }
    }
}

} // namespace xmb_draw
