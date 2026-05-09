// Port of settings_view.lua. Same animation speeds + scroll behavior.
#include "settings_view.h"
#include "assets.h"
#include "common.h"
#include "render.h"
#include "settings.h"
#include "theme.h"
#include "ui.h"
#include "utils.h"
#include <algorithm>
#include <cmath>
#include <dirent.h>
#include <sys/stat.h>

namespace settings_view {

bool active = false;
float alpha = 0;
float scroll_y = 0, target_scroll_y = 0;
int selected_option_idx = 1;

bool picker_active = false;
float picker_alpha = 0;
std::vector<PickerItem> picker_items;
int picker_selected_idx = 1;
std::string picker_current_path = "/";
int picker_setting_idx = -1;
float picker_scroll_y = 0, picker_target_scroll_y = 0;

static std::vector<PickerItem> list_directories(const std::string& path_) {
    std::vector<PickerItem> out;
    std::string path = path_.empty() ? "/" : path_;
    DIR* d = opendir(path.c_str());
    if (!d) return out;
    while (auto* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        std::string full = path + "/" + e->d_name;
        struct stat st;
        if (::stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            out.push_back({e->d_name, full});
        }
    }
    closedir(d);
    std::sort(out.begin(), out.end(), [](const PickerItem& a, const PickerItem& b) {
        std::string la = a.name, lb = b.name;
        for (auto& c : la) c = std::tolower((unsigned char)c);
        for (auto& c : lb) c = std::tolower((unsigned char)c);
        return la < lb;
    });
    return out;
}

void ensure_picker_visible() {
    if (picker_items.empty()) { picker_target_scroll_y = 0; return; }
    int sh = g_app.screen_h;
    float panel_h = std::min(sh * 0.6f, 420.0f);
    float list_h = panel_h - 140;
    float item_h = 36;
    int sel = picker_selected_idx > 0 ? picker_selected_idx : 1;
    float selected_y = (sel - 1) * item_h;
    float desired_top = selected_y - (list_h * 0.5f - item_h * 0.5f);
    float max_scroll = 0;
    float min_scroll = std::min(0.0f, -((float)picker_items.size() * item_h) + list_h);
    float target = -desired_top;
    if (target > max_scroll) target = max_scroll;
    if (target < min_scroll) target = min_scroll;
    picker_target_scroll_y = target;
}

void update(float dt, int setting_idx) {
    if (active) {
        alpha = std::min(1.0f, alpha + dt * 10);
        if (setting_idx >= 0 && setting_idx < (int)settings::options.size()) {
            const auto& opt = settings::options[setting_idx];
            if (!opt.choices.empty()) {
                float item_h = 50;
                float screen_h = (float)g_app.screen_h;
                float visible_area_h = screen_h * 0.6f;
                target_scroll_y = -(selected_option_idx - 1) * item_h
                                + (visible_area_h * 0.5f) - (item_h * 0.5f);
                float max_scroll = 0;
                float min_scroll = std::min(0.0f, -((float)opt.choices.size() * item_h) + visible_area_h);
                target_scroll_y = std::max(min_scroll, std::min(max_scroll, target_scroll_y));
            }
        }
    } else {
        alpha = std::max(0.0f, alpha - dt * 10);
    }
    scroll_y = utils::lerp(scroll_y, target_scroll_y, dt * 10);

    if (picker_active) picker_alpha = std::min(1.0f, picker_alpha + dt * 12);
    else               picker_alpha = std::max(0.0f, picker_alpha - dt * 12);
    picker_scroll_y = utils::lerp(picker_scroll_y, picker_target_scroll_y, dt * 12);
}

void draw_popup(int setting_idx) {
    if (setting_idx < 0 || setting_idx >= (int)settings::options.size()) return;
    const auto& opt = settings::options[setting_idx];
    if (opt.type != settings::OptType::Choice) return;
    if (alpha <= 0) return;

    int sw = g_app.screen_w, sh = g_app.screen_h;
    float panel_w = sw * 0.35f;
    float item_h = 50;
    float a = alpha;
    float x = sw - (panel_w * a);
    float y = 0;

    render::set_color(Color{0.02f, 0.02f, 0.05f, 0.92f * a});
    render::rect_fill(x, y, panel_w, sh);

    render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.8f * a});
    render::rect_fill(x, y, 4, sh);

    float content_y_base = sh * 0.3f;
    float visible_area_h = sh * 0.6f;

    render::set_clip((int)x, (int)content_y_base, (int)panel_w, (int)visible_area_h);
    render::push_translate(0, content_y_base + scroll_y);

    for (size_t i = 0; i < opt.choices.size(); ++i) {
        float cy = (float)i * item_h;
        bool is_selected = ((int)i + 1 == selected_option_idx);
        bool is_current  = ((int)i + 1 == opt.value);

        if (is_selected) {
            render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.3f * a});
            render::rect_fill(x + 4, cy - 5, panel_w - 4, item_h);

            render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 1.0f * a});
            render::rect_fill(x + 4, cy - 5, 4, item_h);

            ui::draw_text(opt.choices[i], x + 40, cy + 5, assets::fonts.small, Color{1, 1, 1, a});
        } else {
            ui::draw_text(opt.choices[i], x + 40, cy + 5, assets::fonts.small, Color{1, 1, 1, 0.6f * a});
        }

        if (is_current) {
            render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 1.0f * a});
            render::circle_fill(x + 25, cy + item_h * 0.5f - 6, 4);
        }
    }

    render::pop_translate();
    render::clear_clip();

    if ((float)opt.choices.size() * item_h > visible_area_h) {
        render::set_color(Color{1, 1, 1, 0.3f * a});
        float centerX = x + panel_w * 0.5f;
        if (scroll_y < 0) {
            SDL_FColor cc{1, 1, 1, 0.3f * a};
            SDL_Vertex v[3];
            v[0] = {{centerX - 10, content_y_base - 14}, cc, {0, 0}};
            v[1] = {{centerX + 10, content_y_base - 14}, cc, {0, 0}};
            v[2] = {{centerX,      content_y_base - 26}, cc, {0, 0}};
            SDL_RenderGeometry(g_app.renderer, nullptr, v, 3, nullptr, 0);
        }
        if (scroll_y > -((float)opt.choices.size() * item_h) + visible_area_h) {
            SDL_FColor cc{1, 1, 1, 0.3f * a};
            float targetY = content_y_base + visible_area_h + 14;
            SDL_Vertex v[3];
            v[0] = {{centerX - 10, targetY},      cc, {0, 0}};
            v[1] = {{centerX + 10, targetY},      cc, {0, 0}};
            v[2] = {{centerX,      targetY + 12}, cc, {0, 0}};
            SDL_RenderGeometry(g_app.renderer, nullptr, v, 3, nullptr, 0);
        }
    }
}

static void rebuild_picker(const std::string& path) {
    picker_current_path = path.empty() ? "/" : path;
    picker_items.clear();
    std::string parent = utils::get_dirname(picker_current_path);
    if (!parent.empty()) picker_items.push_back({"..", parent});
    auto dirs = list_directories(picker_current_path);
    for (auto& d : dirs) picker_items.push_back(d);
    picker_selected_idx = 1;
    ensure_picker_visible();
}

void open_folder_picker(const std::string& initial_path, int setting_idx) {
    picker_active = true;
    picker_setting_idx = setting_idx;
    rebuild_picker(initial_path.empty() ? "/" : initial_path);
    active = true;
}

void close_folder_picker() {
    picker_active = false;
    picker_items.clear();
    picker_selected_idx = 1;
    picker_setting_idx = -1;
    picker_current_path = "/";
    picker_target_scroll_y = 0;
    active = false;
}

void draw_folder_picker() {
    if (picker_alpha <= 0) return;
    int sw = g_app.screen_w, sh = g_app.screen_h;
    float panel_w = sw * 0.70f;
    float panel_h = std::min(sh * 0.8f, 420.0f);
    float x = std::floor(sw * 0.5f) - std::floor(panel_w * 0.5f);
    float y = std::floor(sh * 0.5f) - std::floor(panel_h * 0.5f);
    float a = picker_alpha;

    render::set_color(Color{0.02f, 0.02f, 0.05f, 0.96f * a});
    render::rect_fill_rounded(x, y, panel_w, panel_h, 12);

    render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.35f * a});
    render::rect_line_rounded(x, y, panel_w, panel_h, 12, 2);

    Color tc{theme::text.r, theme::text.g, theme::text.b, a};
    ui::draw_glow_text("Select Folder", x + 20, y + 18, assets::fonts.main, tc, nullptr);
    ui::draw_text(picker_current_path, x + 20, y + 52, assets::fonts.xs, Color{1, 1, 1, 0.6f * a});

    float list_x = x + 20;
    float list_y = y + 92;
    float list_h = panel_h - 140;
    float item_h = 36;

    render::set_clip((int)list_x, (int)list_y, (int)panel_w - 40, (int)list_h);
    render::push_translate(0, list_y + picker_scroll_y);

    for (size_t i = 0; i < picker_items.size(); ++i) {
        float cy = (float)i * item_h;
        bool focused = ((int)i + 1 == picker_selected_idx);
        if (focused) {
            render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, 0.24f * a});
            render::rect_fill_rounded(list_x - 6, cy - 6, panel_w - 28, item_h, 6);
            ui::draw_text(picker_items[i].name, list_x, cy, assets::fonts.small, Color{1, 1, 1, a});
        } else {
            ui::draw_text(picker_items[i].name, list_x, cy, assets::fonts.small, Color{1, 1, 1, 0.8f * a});
        }
    }

    render::pop_translate();
    render::clear_clip();

    float hint_y = y + panel_h - 40;
    ui::draw_text("A: Open    B: Back/Close    X: Set Folder", x + 20, hint_y, assets::fonts.xs, Color{1, 1, 1, 0.85f * a});
}

bool keypressed(const std::string& key) {
    if (!picker_active) return false;
    if (key == "up") {
        picker_selected_idx = std::max(1, picker_selected_idx - 1);
        ensure_picker_visible();
        if (settings::keytone_enabled) assets::play_sfx("nav");
        return true;
    }
    if (key == "down") {
        picker_selected_idx = std::min((int)picker_items.size(), picker_selected_idx + 1);
        ensure_picker_visible();
        if (settings::keytone_enabled) assets::play_sfx("nav");
        return true;
    }
    if (key == "return" || key == "a" || key == "space") {
        if (picker_selected_idx >= 1 && picker_selected_idx <= (int)picker_items.size()) {
            const auto& pick = picker_items[picker_selected_idx - 1];
            if (!pick.path.empty()) {
                rebuild_picker(pick.path);
                if (settings::keytone_enabled) assets::play_sfx("nav");
            }
        }
        return true;
    }
    if (key == "x") {
        if (picker_setting_idx >= 0 && picker_setting_idx < (int)settings::options.size()) {
            settings::options[picker_setting_idx].str_value = picker_current_path;
            settings::apply();
            settings::save();
            ui::show_toast("Media directory set", "folder", "bottom_right");
        }
        close_folder_picker();
        return true;
    }
    if (key == "backspace" || key == "b" || key == "escape") {
        std::string parent = utils::get_dirname(picker_current_path);
        if (parent.empty()) close_folder_picker();
        else {
            rebuild_picker(parent);
            if (settings::keytone_enabled) assets::play_sfx("nav");
        }
        return true;
    }
    return false;
}

} // namespace settings_view
