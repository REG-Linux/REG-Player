// Port of ui.lua. Direct mapping. No shader; gloss icon drawn plain (per anti-drift constraint).
#include "ui.h"
#include "assets.h"
#include "render.h"
#include "theme.h"
#include "utils.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace ui {

// ---- Text rendering with simple texture cache ----
struct TextKey {
    TTF_Font* font;
    std::string text;
    bool operator==(const TextKey& o) const { return font == o.font && text == o.text; }
};
struct TextKeyHash {
    size_t operator()(const TextKey& k) const noexcept {
        return std::hash<const void*>()(k.font) ^ std::hash<std::string>()(k.text);
    }
};
struct TextEntry { SDL_Texture* tex; int w; int h; };

static std::unordered_map<TextKey, TextEntry, TextKeyHash> g_text_cache;

static TextEntry get_text_tex(TTF_Font* font, const std::string& text) {
    if (!font || text.empty()) return {nullptr, 0, 0};
    TextKey k{font, text};
    auto it = g_text_cache.find(k);
    if (it != g_text_cache.end()) return it->second;
    SDL_Color white{255, 255, 255, 255};
    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), text.size(), white);
    if (!surf) return {nullptr, 0, 0};
    SDL_Texture* tex = SDL_CreateTextureFromSurface(g_app.renderer, surf);
    int w = surf->w, h = surf->h;
    SDL_DestroySurface(surf);
    if (!tex) return {nullptr, 0, 0};
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    TextEntry e{tex, w, h};
    g_text_cache.emplace(k, e);
    return e;
}

int text_width(TTF_Font* font, const std::string& s) {
    if (!font || s.empty()) return 0;
    int w = 0, h = 0;
    if (TTF_GetStringSize(font, s.c_str(), s.size(), &w, &h)) return w;
    return 0;
}
int text_height(TTF_Font* font) {
    return font ? TTF_GetFontHeight(font) : 0;
}

void draw_text(const std::string& text, float x, float y, TTF_Font* font, const Color& color) {
    auto e = get_text_tex(font, text);
    if (!e.tex) return;
    SDL_SetTextureColorModFloat(e.tex, color.r, color.g, color.b);
    SDL_SetTextureAlphaModFloat(e.tex, color.a);
    render::draw_texture(e.tex, x, y, (float)e.w, (float)e.h);
}

void draw_text_aligned(const std::string& text, float x, float y, int width, TTF_Font* font, const Color& color, const std::string& align) {
    auto e = get_text_tex(font, text);
    if (!e.tex) return;
    float drawx = x;
    if (align == "right") drawx = x + width - e.w;
    else if (align == "center") drawx = x + (width - e.w) * 0.5f;
    SDL_SetTextureColorModFloat(e.tex, color.r, color.g, color.b);
    SDL_SetTextureAlphaModFloat(e.tex, color.a);
    render::draw_texture(e.tex, drawx, y, (float)e.w, (float)e.h);
}

// ---- Marquee ----
Marquee new_marquee(int max_width, float speed, float pause_start, float pause_end) {
    Marquee m;
    m.max_width = max_width;
    m.speed = speed;
    m.pause_start = pause_start;
    m.pause_end = pause_end;
    return m;
}

void update_marquee(Marquee& m, float dt, int tw) {
    if (tw <= m.max_width) {
        m.offset = 0;
        m.timer = 0;
        m.phase = "pause_start";
        return;
    }
    int max_offset = tw - m.max_width + 20;
    m.timer += dt;
    if (m.phase == "pause_start") {
        if (m.timer > m.pause_start) { m.phase = "scrolling"; m.timer = 0; }
    } else if (m.phase == "scrolling") {
        m.offset = (int)(m.offset + dt * m.speed);
        if (m.offset >= max_offset) {
            m.offset = max_offset;
            m.phase = "pause_end";
            m.timer = 0;
        }
    } else if (m.phase == "pause_end") {
        if (m.timer > m.pause_end) { m.offset = 0; m.phase = "pause_start"; m.timer = 0; }
    }
}

static void draw_text_at(TTF_Font* font, const std::string& text, float x, float y, const Color& c) {
    auto e = get_text_tex(font, text);
    if (!e.tex) return;
    SDL_SetTextureColorModFloat(e.tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaModFloat(e.tex, c.a);
    render::draw_texture(e.tex, x, y, (float)e.w, (float)e.h);
}

void draw_marquee(Marquee& m, const std::string& text, float x, float y,
                  TTF_Font* font, const Color& color, float abs_x, float abs_y,
                  const Color* glow_color) {
    int tw = text_width(font, text);
    int th = text_height(font);
    auto draw_at = [&](float ox, float oy, const Color& c) {
        if (tw > m.max_width) {
            render::set_clip((int)abs_x, (int)abs_y, m.max_width, th);
            draw_text_at(font, text, x - m.offset + ox, y + oy, c);
            render::clear_clip();
        } else {
            draw_text_at(font, text, x + ox, y + oy, c);
        }
    };
    float alpha = color.a;
    float shadow_alpha = theme::shadow_intensity * alpha;
    float glow_mult = (theme::current_mode == "Dark") ? 1.5f : 1.3f;

    for (int i = 1; i <= 4; ++i) {
        int offset = i + 1;
        float layer_alpha = shadow_alpha * (1.1f - i * 0.25f);
        draw_at((float)offset, (float)offset, Color{0, 0, 0, layer_alpha});
    }

    if (glow_color) {
        for (int i = 1; i <= theme::glow_radius; ++i) {
            float layer_alpha = (theme::glow_intensity * glow_mult / i) * alpha;
            Color gc{glow_color->r, glow_color->g, glow_color->b, layer_alpha};
            draw_at((float)-i, 0, gc);
            draw_at((float)i, 0, gc);
            draw_at(0, (float)-i, gc);
            draw_at(0, (float)i, gc);
        }
    }

    draw_at(0, 0, color);
}

void draw_progress_bar(float x, float y, float w, float h, float progress, const Color& color, const Color& bg_color) {
    render::set_color(bg_color);
    render::rect_fill_rounded(x, y, w, h, 2.0f);
    render::set_color(color);
    render::rect_fill_rounded(x, y, w * progress, h, 2.0f);
}

void draw_icon(SDL_Texture* icon, float x, float y, float size, const Color& color, float alpha, SDL_Texture* thumbnail) {
    SDL_Texture* img = thumbnail ? thumbnail : icon;
    if (!img) return;
    float iw, ih;
    SDL_GetTextureSize(img, &iw, &ih);
    float scale = size / std::max(iw, ih);
    if (thumbnail) {
        SDL_SetTextureColorModFloat(img, 1.0f, 1.0f, 1.0f);
        SDL_SetTextureAlphaModFloat(img, alpha);
    } else {
        SDL_SetTextureColorModFloat(img, color.r, color.g, color.b);
        SDL_SetTextureAlphaModFloat(img, (color.a * alpha));
    }
    render::draw_texture_scaled(img, x, y, scale, 0.5f, 0.5f);
}

void draw_glow_text(const std::string& text, float x, float y, TTF_Font* font, const Color& color,
                    const Color* glow_color, int limit, const std::string& align) {
    float alpha = color.a;
    float shadow_alpha = theme::shadow_intensity * alpha;
    float glow_mult = (theme::current_mode == "Dark") ? 1.5f : 1.3f;

    auto draw_at = [&](float ox, float oy, const Color& c) {
        if (limit > 0 && !align.empty()) {
            draw_text_aligned(text, x + ox, y + oy, limit, font, c, align);
        } else {
            draw_text_at(font, text, x + ox, y + oy, c);
        }
    };

    for (int i = 1; i <= 4; ++i) {
        float offset = (float)(i + 1);
        float layer_alpha = shadow_alpha * (1.1f - i * 0.25f);
        draw_at(offset, offset, Color{0, 0, 0, layer_alpha});
    }

    if (glow_color) {
        for (int i = 1; i <= theme::glow_radius; ++i) {
            float layer_alpha = (theme::glow_intensity * glow_mult / i) * alpha;
            Color gc{glow_color->r, glow_color->g, glow_color->b, layer_alpha};
            draw_at((float)-i, 0, gc);
            draw_at((float)i, 0, gc);
            draw_at(0, (float)-i, gc);
            draw_at(0, (float)i, gc);
        }
    }

    draw_at(0, 0, color);
}

void draw_glow_icon(SDL_Texture* icon, float x, float y, float size, const Color& color, float alpha,
                    const Color* glow_color, SDL_Texture* thumbnail) {
    SDL_Texture* img = thumbnail ? thumbnail : icon;
    if (!img) return;
    float iw, ih;
    SDL_GetTextureSize(img, &iw, &ih);
    float base_scale = size / std::max(iw, ih);

    if (thumbnail) {
        SDL_SetTextureColorModFloat(img, 1.0f, 1.0f, 1.0f);
        SDL_SetTextureAlphaModFloat(img, alpha);
        render::draw_texture_scaled(img, x, y, base_scale, 0.5f, 0.5f);
        return;
    }

    float a = alpha;
    float shadow_alpha = theme::shadow_intensity * a;
    float glow_base = (theme::current_mode == "Dark") ? (theme::glow_intensity * 1.2f) : (theme::glow_intensity * 1.1f);

    // Shadow layers
    for (int i = 1; i <= 4; ++i) {
        float offset = (float)(i + 1);
        float layer_alpha = shadow_alpha * (1.1f - i * 0.25f);
        SDL_SetTextureColorModFloat(img, 0.0f, 0.0f, 0.0f);
        SDL_SetTextureAlphaModFloat(img, layer_alpha);
        render::draw_texture_scaled(img, x + offset, y + offset, base_scale, 0.5f, 0.5f);
    }

    // Glow layers
    if (glow_color) {
        double t = SDL_GetTicksNS() / 1.0e9;
        float pulse = 0.8f + 0.2f * std::sin(t * 3.0);
        for (int i = 1; i <= theme::glow_radius; ++i) {
            float layer_alpha = (glow_base * 0.8f / std::pow((float)i, 1.2f)) * a * pulse;
            float scale = base_scale * (1.0f + i * 0.05f) * (0.98f + pulse * 0.02f);
            SDL_SetTextureColorModFloat(img, glow_color->r, glow_color->g, glow_color->b);
            SDL_SetTextureAlphaModFloat(img, layer_alpha);
            render::draw_texture_scaled(img, x, y, scale, 0.5f, 0.5f);
        }
    }

    // Main
    SDL_SetTextureColorModFloat(img, color.r, color.g, color.b);
    SDL_SetTextureAlphaModFloat(img, a);
    render::draw_texture_scaled(img, x, y, base_scale, 0.5f, 0.5f);
}

// ---- Toasts ----
struct Toast {
    std::string text;
    SDL_Texture* icon = nullptr;
    std::string position = "top_center";
    std::string type;
    float timer = 3.0f;
    float fade_time = 0.3f;
    float alpha = 0;
    int target_level = 0;
    float display_level = 0;
};

static std::vector<Toast> g_toasts;

void show_toast(const std::string& text, const std::string& icon_name, const std::string& position) {
    Toast t;
    t.text = text;
    if (!icon_name.empty()) t.icon = assets::get_image(icon_name);
    t.position = position.empty() ? "top_center" : position;
    g_toasts.push_back(t);
}

void show_volume_toast(int volume) {
    for (auto& t : g_toasts) {
        if (t.type == "volume") { t.target_level = volume; t.timer = 1.5f; return; }
    }
    Toast t;
    t.type = "volume";
    t.target_level = volume;
    t.display_level = (float)volume;
    t.position = "top_center";
    t.timer = 1.5f;
    g_toasts.push_back(t);
}

void show_brightness_toast(int brightness) {
    for (auto& t : g_toasts) {
        if (t.type == "brightness") { t.target_level = brightness; t.timer = 1.5f; return; }
    }
    Toast t;
    t.type = "brightness";
    t.target_level = brightness;
    t.display_level = (float)brightness;
    t.position = "top_center";
    t.timer = 1.5f;
    g_toasts.push_back(t);
}

void update_toasts(float dt) {
    for (size_t i = g_toasts.size(); i > 0; --i) {
        Toast& t = g_toasts[i - 1];
        t.timer -= dt;
        if (t.type == "volume" || t.type == "brightness") {
            t.display_level = utils::lerp(t.display_level, (float)t.target_level, dt * 10.0f);
        }
        if (t.timer > 2.7f) t.alpha = (3.0f - t.timer) / 0.3f;
        else if (t.timer < 0.3f) t.alpha = t.timer / 0.3f;
        else t.alpha = 1.0f;
        if (t.timer <= 0) g_toasts.erase(g_toasts.begin() + (i - 1));
    }
}

void draw_toasts() {
    int sw = g_app.screen_w, sh = g_app.screen_h;
    TTF_Font* font = assets::fonts.small;
    const int padding = 15;
    const int icon_size = 28;

    for (auto& t : g_toasts) {
        float box_w, box_h, x, y;

        if (t.type == "volume" || t.type == "brightness") {
            box_w = 260;
            box_h = 45;
            x = (sw - box_w) * 0.5f;
            y = 4 + (1 - t.alpha) * -20;

            render::set_color(Color{0.1f, 0.1f, 0.15f, 0.85f * t.alpha});
            render::rect_fill_rounded(x, y, box_w, box_h, 12);

            render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.5f * t.alpha});
            render::rect_line_rounded(x, y, box_w, box_h, 12, 2);

            SDL_Texture* icon_img = nullptr;
            int level = (int)t.display_level;
            int max_level = (t.type == "brightness") ? 255 : 100;
            if (t.type == "volume") {
                if (t.target_level == 0) icon_img = assets::get_image("volume_mute");
                else if (t.target_level < 50) icon_img = assets::get_image("volume_down");
                else icon_img = assets::get_image("volume_up");
            } else {
                icon_img = assets::get_image("brightness");
            }
            if (icon_img) draw_icon(icon_img, x + padding + icon_size * 0.5f, y + box_h * 0.5f, (float)icon_size, Color{1, 1, 1, 1}, t.alpha);

            float bar_x = x + padding + icon_size + 15;
            float bar_w = box_w - (padding * 2 + icon_size + 15);
            float bar_h = 8;
            float bar_y = y + (box_h - bar_h) * 0.5f;
            float progress = std::max(0.0f, std::min(1.0f, level / (float)max_level));
            draw_progress_bar(bar_x, bar_y, bar_w, bar_h, progress,
                Color{1, 1, 1, 0.9f * t.alpha},
                Color{1, 1, 1, 0.2f * t.alpha});
        } else {
            int tw = text_width(font, t.text);
            int th = text_height(font);
            box_w = tw + padding * 2;
            if (t.icon) box_w += icon_size + 10;
            box_h = std::max(th, icon_size) + padding;

            if (t.position == "top_center") {
                x = (sw - box_w) * 0.5f;
                y = 4 + (1 - t.alpha) * -20;
            } else { // bottom_right
                x = sw - box_w - 30;
                y = sh - box_h - 30 + (1 - t.alpha) * 20;
            }

            render::set_color(Color{0.1f, 0.1f, 0.15f, 0.85f * t.alpha});
            render::rect_fill_rounded(x, y, box_w, box_h, 12);

            render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.5f * t.alpha});
            render::rect_line_rounded(x, y, box_w, box_h, 12, 2);

            float cur_x = x + padding;
            if (t.icon) {
                draw_icon(t.icon, cur_x + icon_size * 0.5f, y + box_h * 0.5f, (float)icon_size, Color{1, 1, 1, 1}, t.alpha);
                cur_x += icon_size + 10;
            }

            draw_text(t.text, cur_x, y + (box_h - th) * 0.5f, font, Color{1, 1, 1, t.alpha});
        }
    }
}

void draw_indexing_popup(const std::string& progress_text, bool final_message) {
    int sw = g_app.screen_w, sh = g_app.screen_h;
    Color accent = theme::accent;
    double t = SDL_GetTicksNS() / 1.0e9;

    TTF_Font* title_font = assets::fonts.large;
    int tfh = text_height(title_font);
    Color white{1, 1, 1, 1};
    draw_glow_text("REG-Player", 0, sh * 0.5f - tfh * 0.5f, title_font, white, &accent, sw, "center");

    Color sub{1, 1, 1, 0.75f};
    draw_text("v0.1 Beta", 20, sh - 36, assets::fonts.small, sub);

    std::string status_text;
    if (final_message) {
        status_text = progress_text.empty() ? "Scanning media..." : progress_text;
    } else {
        int dots_n = ((int)(t * 2) % 3) + 1;
        std::string dots(dots_n, '.');
        status_text = "Indexing" + dots + "  " + (progress_text.empty() ? "Scanning media..." : progress_text);
    }

    TTF_Font* sf = assets::fonts.xs ? assets::fonts.xs : assets::fonts.small;
    int sw2 = text_width(sf, status_text);
    float pulse = 0.62f + (std::sin(t * 3.0) + 1) * 0.14f;
    Color pc{accent.r, accent.g, accent.b, pulse};
    draw_text(status_text, sw - sw2 - 20, sh - 36, sf, pc);
}

} // namespace ui
