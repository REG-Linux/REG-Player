// Port of music_view.lua. Same display sleep behavior, lock combo, options popup.
#include "music_view.h"
#include "assets.h"
#include "background.h"
#include "common.h"
#include "gamepad.h"
#include "music_player.h"
#include "render.h"
#include "settings.h"
#include "theme.h"
#include "ui.h"
#include "utils.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace music_view {

bool buttons_locked = false;
static float idle_seconds = 0;
static float display_sleep_alpha = 0;
static bool  display_sleeping = false;

struct ContextMenu {
    bool active = false;
    float alpha = 0;
    int selected_idx = 1;
    std::vector<std::string> items = {"repeat_one", "display_sleep", "auto_sleep", "visualizer"};
};
static ContextMenu cm;

static const std::unordered_set<std::string>& hold_wake_blocked_keys() {
    static const std::unordered_set<std::string> s = {"pageup", "pagedown", "l", "e"};
    return s;
}

static std::string repeat_label() { return music_player::s.repeat_one ? "On" : "Off"; }

static std::string auto_sleep_label() {
    if (music_player::s.auto_sleep_minutes <= 0) return "Off";
    char buf[16]; std::snprintf(buf, sizeof(buf), "%dm", music_player::s.auto_sleep_minutes);
    return buf;
}

static std::string display_sleep_label() {
    auto* opt = settings::get_option("display_sleep");
    if (!opt || opt->choices.empty()) return "Off";
    int idx = std::max(1, std::min((int)opt->choices.size(), opt->value)) - 1;
    return opt->choices[idx];
}

static std::string visualizer_label() {
    if (music_player::s.visualizer_mode == "off")  return "Off";
    if (music_player::s.visualizer_mode == "bars") return "Bars";
    return "Wave";
}

static void cycle_current_option(int direction) {
    if (cm.selected_idx < 1 || cm.selected_idx > (int)cm.items.size()) return;
    const std::string& selected = cm.items[cm.selected_idx - 1];
    if (selected == "repeat_one") {
        music_player::set_repeat_one(!music_player::s.repeat_one);
    } else if (selected == "display_sleep") {
        if (auto* opt = settings::get_option("display_sleep")) {
            int n = (int)opt->choices.size();
            if (n > 0) {
                int v = opt->value + direction;
                if (v > n) v = 1;
                if (v < 1) v = n;
                opt->value = v;
                settings::apply();
                settings::save();
            }
        }
    } else if (selected == "auto_sleep") {
        int v = music_player::s.auto_sleep_minutes + direction;
        if (v > 30) v = 0;
        if (v < 0)  v = 30;
        music_player::set_auto_sleep_minutes(v);
    } else if (selected == "visualizer") {
        static const char* modes[] = {"off", "wave", "bars"};
        int idx = 1;
        for (int i = 0; i < 3; ++i) if (music_player::s.visualizer_mode == modes[i]) { idx = i + 1; break; }
        idx += direction;
        if (idx > 3) idx = 1;
        if (idx < 1) idx = 3;
        music_player::set_visualizer_mode(modes[idx - 1]);
    }
}

void init() {
    int info_w = (int)(g_app.screen_w * 0.65f);
    music_player::s.marquees["title"]  = ui::new_marquee(info_w);
    music_player::s.marquees["artist"] = ui::new_marquee(info_w);
    music_player::s.marquees["album"]  = ui::new_marquee(info_w);
    buttons_locked = false;
}

void on_music_opened() {
    idle_seconds = 0;
    display_sleep_alpha = 0;
    display_sleeping = false;
}
void on_music_closed() {
    idle_seconds = 0;
    display_sleep_alpha = 0;
    display_sleeping = false;
}
void register_user_input() {
    idle_seconds = 0;
    display_sleeping = false;
}
bool is_display_sleeping() { return display_sleep_alpha > 0.01f; }

void update(float dt) {
    if (!music_player::s.active) return;
    int sleep_seconds = settings::display_sleep_seconds;
    if (sleep_seconds <= 0) {
        idle_seconds = 0;
        display_sleeping = false;
        display_sleep_alpha = std::max(0.0f, display_sleep_alpha - dt * 6);
        return;
    }
    idle_seconds += dt;
    if (idle_seconds >= sleep_seconds) {
        display_sleep_alpha = std::min(1.0f, display_sleep_alpha + dt * 2);
    } else {
        display_sleep_alpha = std::max(0.0f, display_sleep_alpha - dt * 6);
    }
    display_sleeping = (display_sleep_alpha >= 0.99f);
}

void draw() {
    if (!music_player::s.active) return;
    int w = g_app.screen_w, h = g_app.screen_h;
    float a = music_player::s.fade_alpha;

    // Header
    Color hc{theme::text.r, theme::text.g, theme::text.b, 0.7f * a};
    ui::draw_text("REG-Player", 20, 10, assets::fonts.small, hc);

    if (!music_player::s.playlist.empty()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d of %zu", music_player::s.current_index + 1, music_player::s.playlist.size());
        ui::draw_text_aligned(buf, 0, 10, w - 16, assets::fonts.small, hc, "right");
    }

    render::set_color(Color{theme::text.r, theme::text.g, theme::text.b, 0.1f * a});
    render::rect_fill(20, 40, w - 40, 1);

    // Album art panel
    float art_size = std::min((float)w, (float)h) * 0.3f;
    float art_x = w * 0.05f, art_y = 60;

    if (background::has_custom_wallpaper()) {
        float panel_x = w * 0.03f;
        float panel_y = art_y - 14;
        float panel_w = w * 0.94f;
        float panel_h = art_size + 24;
        render::set_color(Color{0.04f, 0.05f, 0.07f, 0.42f * a});
        render::rect_fill_rounded(panel_x, panel_y, panel_w, panel_h, 14);
        render::set_color(Color{theme::text.r, theme::text.g, theme::text.b, 0.08f * a});
        render::rect_line_rounded(panel_x, panel_y, panel_w, panel_h, 14, 2);
    }

    SDL_Texture* cover = (SDL_Texture*)music_player::s.cover_art;
    if (cover) {
        float iw, ih;
        SDL_GetTextureSize(cover, &iw, &ih);
        float scale = art_size / std::max(iw, ih);
        float draw_w = iw * scale, draw_h = ih * scale;
        float ox = (art_size - draw_w) * 0.5f;
        float oy = (art_size - draw_h) * 0.5f;
        render::set_color(Color{theme::text.r, theme::text.g, theme::text.b, 0.1f * a});
        render::rect_fill_rounded(art_x - 4, art_y - 4, art_size + 8, art_size + 8, 6);
        SDL_SetTextureColorModFloat(cover, 1, 1, 1);
        SDL_SetTextureAlphaModFloat(cover, a);
        render::draw_texture(cover, art_x + ox, art_y + oy, draw_w, draw_h);
    } else {
        render::set_color(Color{theme::text.r, theme::text.g, theme::text.b, 0.1f * a});
        render::rect_fill_rounded(art_x - 4, art_y - 4, art_size + 8, art_size + 8, 6);
        ui::draw_icon(assets::get_image("music"),
            art_x + art_size * 0.5f, art_y + art_size * 0.5f,
            art_size * 0.4f, theme::text, 0.2f * a);
    }

    // Track info
    float info_x = w * 0.3f;
    float info_y = 60;
    float info_w = w * 0.65f;
    float extra_y = info_y + 120;

    // Title / artist / album
    auto& mq = music_player::s.marquees;
    std::string track_name = !music_player::s.tags.title.empty()
        ? music_player::s.tags.title : utils::get_track_name(music_player::s.current_track.name);
    Color tc{theme::text.r, theme::text.g, theme::text.b, a};
    if (mq.count("title")) {
        ui::draw_marquee(mq["title"], track_name, info_x, info_y, assets::fonts.title, tc, info_x, info_y);
    }
    render::set_color(Color{theme::text.r, theme::text.g, theme::text.b, 0.1f * a});
    render::rect_fill(info_x, info_y + 40, info_w, 1);

    Color ac{theme::text.r, theme::text.g, theme::text.b, 0.7f * a};
    std::string artist = !music_player::s.tags.artist.empty() ? music_player::s.tags.artist : "Unknown Artist";
    if (mq.count("artist")) ui::draw_marquee(mq["artist"], artist, info_x, info_y + 46, assets::fonts.artist, ac, info_x, info_y + 46);

    Color alc{theme::text.r, theme::text.g, theme::text.b, 0.4f * a};
    std::string album = !music_player::s.tags.album.empty() ? music_player::s.tags.album : "Unknown Album";
    if (mq.count("album")) ui::draw_marquee(mq["album"], album, info_x, info_y + 74, assets::fonts.album, alc, info_x, info_y + 74);

    // Next track
    if (music_player::s.playlist.size() > 1) {
        int next_idx = (music_player::s.current_index + 1) % (int)music_player::s.playlist.size();
        const auto& nt = music_player::s.playlist[next_idx];
        std::string nt_title = !music_player::s.next_tags.title.empty()
            ? music_player::s.next_tags.title : utils::get_track_name(nt.name);
        std::string s = "Next: " + nt_title;
        std::string disp = utils::truncate_text(s, assets::fonts.small, (int)(info_w * 0.8f));
        ui::draw_text(disp, info_x, extra_y, assets::fonts.small, Color{theme::text.r, theme::text.g, theme::text.b, 0.4f * a});
    }

    std::string ext = utils::get_extension(music_player::s.current_track.path);
    if (!ext.empty()) {
        std::string up = ext.substr(1);
        for (auto& c : up) c = std::toupper((unsigned char)c);
        ui::draw_text_aligned(up, info_x, extra_y, (int)info_w, assets::fonts.small,
            Color{theme::text.r, theme::text.g, theme::text.b, 0.3f * a}, "right");
    }

    SDL_Texture* status_icon = music_player::s.paused ? assets::get_image("pause") : assets::get_image("play");
    ui::draw_icon(status_icon, 48, h - 42, 48, theme::text, 0.8f * a);

    if (buttons_locked && assets::get_image("lock")) {
        ui::draw_icon(assets::get_image("lock"), 48, h - 100, 48, theme::text, 0.95f * a);
    }
    if (music_player::s.repeat_one && assets::get_image("repeat_one")) {
        ui::draw_icon(assets::get_image("repeat_one"), 100, h - 42, 36, theme::text, 0.8f * a);
    }

    // Progress bar
    float bar_w = w * 0.4f, bar_h = 4;
    float bar_x = w - bar_w - 30, bar_y = h - 30;
    float progress = (music_player::s.duration > 0) ? (float)(music_player::s.elapsed / music_player::s.duration) : 0.0f;

    ui::draw_text(utils::format_time(music_player::s.elapsed),
        bar_x + bar_w - 140, bar_y - 34, assets::fonts.time_elapsed,
        Color{theme::text.r, theme::text.g, theme::text.b, 0.9f * a});

    ui::draw_text_aligned("/ " + utils::format_time(music_player::s.duration),
        0, bar_y - 30, (int)(bar_x + bar_w), assets::fonts.time_dur,
        Color{theme::text.r, theme::text.g, theme::text.b, 0.9f * a}, "right");

    ui::draw_progress_bar(bar_x, bar_y, bar_w, bar_h, progress,
        Color{0.55f, 0.65f, 1.0f, 0.9f * a},
        Color{theme::text.r, theme::text.g, theme::text.b, 0.1f * a});

    // Options menu
    if (cm.active) cm.alpha = std::min(1.0f, cm.alpha + 0.2f);
    else           cm.alpha = std::max(0.0f, cm.alpha - 0.2f);

    if (cm.alpha > 0) {
        float menu_w = 300, row_h = 44;
        float menu_h = ((float)cm.items.size() * row_h) + 28;
        float menu_x = w - menu_w - 20;
        float menu_y = bar_y - menu_h - 48;

        render::set_color(Color{0.05f, 0.05f, 0.08f, 0.92f * cm.alpha});
        render::rect_fill_rounded(menu_x, menu_y, menu_w, menu_h, 14);

        render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.3f * cm.alpha});
        render::rect_line_rounded(menu_x, menu_y, menu_w, menu_h, 14, 2);

        for (size_t i = 0; i < cm.items.size(); ++i) {
            float yy = menu_y + 14 + (float)i * row_h;
            bool focused = ((int)i + 1 == cm.selected_idx);
            std::string label, value;
            const std::string& id = cm.items[i];
            if      (id == "repeat_one")    { label = "Repeat One";    value = repeat_label(); }
            else if (id == "display_sleep") { label = "Display Sleep"; value = display_sleep_label(); }
            else if (id == "auto_sleep")    { label = "Auto Sleep";    value = auto_sleep_label(); }
            else if (id == "visualizer")    { label = "Visualizer";    value = visualizer_label(); }

            if (focused) {
                render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.25f * cm.alpha});
                render::rect_fill_rounded(menu_x + 10, yy - 4, menu_w - 20, row_h - 4, 9);
                ui::draw_glow_text(label, menu_x + 24, yy + 2, assets::fonts.small,
                    Color{theme::text.r, theme::text.g, theme::text.b, cm.alpha}, nullptr);
            } else {
                ui::draw_text(label, menu_x + 24, yy + 2, assets::fonts.small,
                    Color{theme::text.r, theme::text.g, theme::text.b, 0.75f * cm.alpha});
            }
            ui::draw_text_aligned(value, menu_x + 20, yy + 2, (int)(menu_w - 44), assets::fonts.small,
                Color{theme::text.r, theme::text.g, theme::text.b, 0.85f * cm.alpha}, "right");
        }
    }

    if (display_sleep_alpha > 0) {
        render::set_color(Color{0, 0, 0, display_sleep_alpha});
        render::rect_fill(0, 0, w, h);
    }
}

bool keypressed(const std::string& key) {
    if (!music_player::s.active) return false;

    bool was_sleeping = display_sleeping;
    bool should_ignore_wake = buttons_locked && hold_wake_blocked_keys().count(key) > 0;

    bool lock_combo = (key == "y" && gamepad::is_logical_down("right"))
                   || (key == "right" && gamepad::is_logical_down("y"));

    if (!should_ignore_wake) register_user_input();
    if (lock_combo) {
        buttons_locked = !buttons_locked;
        if (buttons_locked) cm.active = false;
        return true;
    }
    if (was_sleeping) return true;
    if (buttons_locked) return true;

    if (cm.active) {
        if (key == "up")    { cm.selected_idx = std::max(1, cm.selected_idx - 1); return true; }
        if (key == "down")  { cm.selected_idx = std::min((int)cm.items.size(), cm.selected_idx + 1); return true; }
        if (key == "left")  { cycle_current_option(-1); return true; }
        if (key == "right" || key == "a" || key == "return") { cycle_current_option(1); return true; }
        if (key == "b" || key == "backspace" || key == "x") { cm.active = false; return true; }
    }

    if (key == "a" || key == "return") { music_player::toggle_pause(); return true; }
    if (key == "x") { cm.active = true; return true; }
    if (key == "right") { music_player::next_track(); return true; }
    if (key == "left")  { music_player::prev_track(); return true; }
    if (key == "b" || key == "backspace") { music_player::close(); return true; }
    return false;
}

} // namespace music_view
