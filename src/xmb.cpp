#include "xmb.h"
#include "assets.h"
#include "browser.h"
#include "categories.h"
#include "common.h"
#include "gamepad.h"
#include "history.h"
#include "image_viewer.h"
#include "indexing.h"
#include "music_player.h"
#include "player.h"
#include "settings.h"
#include "settings_view.h"
#include "theme.h"
#include "ui.h"
#include "utils.h"
#include "video_manager.h"
#include "background.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace xmb {

int current_category_idx = 3; // Music (1-based) — matches Lua default 3
int current_item_idx = 1;
float category_scroll_x = 0, target_category_scroll_x = 0;
float item_scroll_y = 0, target_item_scroll_y = 0;
ui::Marquee item_marquee = ui::new_marquee(0, 50, 1.5f, 1.0f);
float list_slide_x = 0;
float list_slide_alpha = 1;
std::vector<int> nav_stack;
std::string view_type = "browser";
void* view_data = nullptr;
ContextMenu context_menu;
bool help_overlay_active = false;
float help_overlay_alpha = 0;

static const float REPEAT_DELAY = 0.4f;
static const float REPEAT_INTERVAL = 0.08f;
static float repeat_timer = 0;
static std::string last_key;

static const std::vector<std::string>& exts(const std::string& kind) {
    return indexing::compatible_extensions(kind);
}

static bool is_in_filter(const std::string& path, const std::string& kind) {
    std::string ext = utils::get_extension(path);
    if (ext.empty()) return false;
    for (auto& e : exts(kind)) if (ext == e) return true;
    return false;
}

static const std::string& current_filter() {
    static const std::string none = "";
    auto& cats = categories::list();
    if (current_category_idx < 1 || current_category_idx > (int)cats.size()) return none;
    return cats[current_category_idx - 1].filter;
}

static int count_media_in_dir(const std::string& dir, const std::string& kind) {
    if (dir.empty()) return 0;
    std::lock_guard<std::mutex> lock(indexing::data_mutex);
    int cnt = 0;
    if (kind == "music") {
        for (auto& kv : indexing::data.music_files)
            if (utils::is_subpath(dir, kv.first)) ++cnt;
    } else if (kind == "photo") {
        for (auto& kv : indexing::data.photos)
            if (utils::is_subpath(dir, kv.first)) ++cnt;
    } else if (kind == "video") {
        for (auto& p : indexing::data.videos)
            if (utils::is_subpath(dir, p)) ++cnt;
    }
    return cnt;
}

static void prep_files() {
    int screen_w = g_app.screen_w;
    float cat_base_x = screen_w * 0.25f;
    int max_w = screen_w - (int)cat_base_x - 40;
    TTF_Font* font = assets::fonts.small;
    for (auto& it : browser::files) {
        it.name = utils::clean_utf8(it.name);
        it.display_name = utils::truncate_text(it.name, font, max_w);
        if (it.type == "file" && it.icon.empty()) {
            if (is_in_filter(it.path, "photo")) it.icon = "photo";
            else if (is_in_filter(it.path, "video")) it.icon = "file_video";
            else if (is_in_filter(it.path, "music")) it.icon = "file_music";
            else it.icon = "file";
        }
    }
}

static void refresh_settings_items(bool keep_idx) {
    int old_idx = keep_idx ? current_item_idx : -1;
    auto bi = settings::get_browser_items();
    std::vector<browser::File> fs;
    for (auto& it : bi) {
        browser::File f;
        f.name = it.name;
        f.type = it.type;
        f.setting_idx = it.setting_idx;
        f.group_idx = it.group_idx;
        f.group_id = it.group_id;
        f.icon = it.icon;
        fs.push_back(f);
    }
    browser::set_files(fs);
    prep_files();
    if (old_idx > 0) current_item_idx = old_idx;
}

static void close_context_menu() {
    context_menu.active = false;
    context_menu.items.clear();
    context_menu.selected_idx = 1;
    context_menu.title.clear();
    context_menu.target_path.clear();
}

static void open_context_menu(const std::string& title, std::vector<ContextMenuItem> items, const std::string& path) {
    if (path.empty() || items.empty()) return;
    context_menu.active = true;
    context_menu.selected_idx = 1;
    context_menu.title = title;
    context_menu.items = std::move(items);
    context_menu.target_path = path;
}

static void open_photo_context_menu(const std::string& path) {
    open_context_menu("Photo Options", {{"set_wallpaper", "Set as Wallpaper"}}, path);
}

static void open_video_context_menu(const std::string& path) {
    bool watched = video_manager::is_watched(path);
    open_context_menu("Video Options",
        {{"toggle_watched", watched ? "Unmark as Watched" : "Mark as Watched"}}, path);
}

static void apply_context_action(const std::string& action_id) {
    if (action_id == "set_wallpaper" && !context_menu.target_path.empty()) {
        const std::string& path = context_menu.target_path;
        if (auto* o = settings::get_option("custom_bg_path")) o->str_value = path;
        if (auto* o = settings::get_option("custom_bg")) o->value = 1;
        settings::apply();
        settings::save();
    } else if (action_id == "toggle_watched" && !context_menu.target_path.empty()) {
        video_manager::toggle_watched(context_menu.target_path);
    }
}

bool in_submenu() {
    auto& cats = categories::list();
    if (current_category_idx < 1 || current_category_idx > (int)cats.size()) return false;
    auto& cat = cats[current_category_idx - 1];
    if (cat.id == "settings") return settings::in_submenu();
    if ((cat.id == "music" || cat.id == "video" || cat.id == "photo" || cat.id == "folder") && view_type != "category_root") return true;
    if (!cat.path.empty()) return utils::normalize_path(browser::current_dir) != utils::normalize_path(browser::base_dir);
    return false;
}

void refresh_items() {
    auto& cats = categories::list();
    auto& cat = cats[current_category_idx - 1];
    browser::set_files({});

    if ((cat.id == "music" || cat.id == "video" || cat.id == "photo") && cat.path.empty()) {
        browser::File f; f.name = "Media directory not set. Please set directory from Settings."; f.type = "info_text";
        browser::set_files({f});
        return;
    }

    auto& files = browser::files;

    if (cat.id == "music") {
        if (view_type == "category_root") {
            browser::File a; a.name = "Albums"; a.type = "view_trigger"; a.target_view = "music_albums"; a.icon = "albums";
            files.push_back(a);
            browser::File b; b.name = "Artists"; b.type = "view_trigger"; b.target_view = "music_artists"; b.icon = "mic";
            files.push_back(b);
            int music_count = 0;
            { std::lock_guard<std::mutex> l(indexing::data_mutex); music_count = (int)indexing::data.music_files.size(); }
            browser::File c; c.name = "Music Files"; c.type = "directory_trigger"; c.path = cat.path; c.icon = "folder_music";
            c.description = std::to_string(music_count) + " tracks";
            files.push_back(c);
            browser::set_state(cat.path, cat.path, cat.filter);
        } else if (view_type == "music_albums") {
            std::lock_guard<std::mutex> l(indexing::data_mutex);
            for (auto& kv : indexing::data.music_albums) {
                browser::File f;
                f.name = kv.second.name;
                f.type = "album";
                f.description = kv.second.artist;
                f.data = (void*)&kv.second;
                files.push_back(f);
            }
            std::sort(files.begin(), files.end(), [](const browser::File& a, const browser::File& b) {
                std::string la = a.name, lb = b.name;
                for (auto& c : la) c = std::tolower((unsigned char)c);
                for (auto& c : lb) c = std::tolower((unsigned char)c);
                if (la != lb) return la < lb;
                std::string da = a.description, db = b.description;
                for (auto& c : da) c = std::tolower((unsigned char)c);
                for (auto& c : db) c = std::tolower((unsigned char)c);
                return da < db;
            });
        } else if (view_type == "music_artists") {
            std::lock_guard<std::mutex> l(indexing::data_mutex);
            for (auto& kv : indexing::data.music_artists) {
                browser::File f;
                f.name = kv.first;
                f.type = "artist";
                f.data = (void*)&kv.second;
                files.push_back(f);
            }
            std::sort(files.begin(), files.end(), [](const browser::File& a, const browser::File& b) {
                std::string la = a.name, lb = b.name;
                for (auto& c : la) c = std::tolower((unsigned char)c);
                for (auto& c : lb) c = std::tolower((unsigned char)c);
                return la < lb;
            });
        } else if (view_type == "album_tracks" || view_type == "artist_tracks") {
            std::vector<std::string> tracks;
            if (view_data) {
                if (view_type == "album_tracks") {
                    auto* a = (indexing::AlbumEntry*)view_data;
                    tracks = a->tracks;
                } else {
                    auto* ar = (indexing::ArtistEntry*)view_data;
                    tracks = ar->tracks;
                }
            }
            browser::File sp; sp.name = "Shuffle Play"; sp.type = "shuffle_play"; sp.icon = "shuffle"; sp.tracks = tracks;
            files.push_back(sp);
            std::lock_guard<std::mutex> l(indexing::data_mutex);
            for (auto& path : tracks) {
                browser::File f; f.path = path; f.type = "file";
                auto it = indexing::data.music_files.find(path);
                f.name = (it != indexing::data.music_files.end() && !it->second.title.empty()) ? it->second.title : "Unknown";
                files.push_back(f);
            }
        } else if (view_type == "browser") {
            browser::scan();
            bool has_music = false;
            for (auto& it : files) {
                if (it.type == "file" && is_in_filter(it.path, "music")) has_music = true;
                else if (it.type == "directory") {
                    int cnt = count_media_in_dir(it.path, "music");
                    it.description = std::to_string(cnt) + " tracks";
                }
            }
            if (has_music) {
                browser::File s; s.name = "Shuffle Play"; s.type = "shuffle_play"; s.icon = "shuffle";
                files.insert(files.begin(), s);
            }
        }
    } else if (cat.id == "video") {
        if (view_type == "category_root") {
            browser::File r; r.name = "Resume Watching"; r.type = "view_trigger"; r.target_view = "video_resume";
            r.icon = "history"; r.description = "Pick up where you left off";
            files.push_back(r);
            int n = 0;
            { std::lock_guard<std::mutex> l(indexing::data_mutex); n = (int)indexing::data.videos.size(); }
            browser::File c; c.name = "Video Files"; c.type = "directory_trigger"; c.path = cat.path; c.icon = "folder_video";
            c.description = std::to_string(n) + " videos";
            files.push_back(c);
            browser::set_state(cat.path, cat.path, cat.filter);
        } else if (view_type == "video_resume") {
            for (auto& it : video_manager::get_resume_list()) {
                browser::File f; f.name = it.name; f.path = it.path; f.type = "file";
                files.push_back(f);
            }
        } else if (view_type == "browser") {
            browser::scan();
            bool has_videos = false;
            for (auto& it : files) {
                if (it.type == "file") has_videos = true;
                else if (it.type == "directory") {
                    int cnt = count_media_in_dir(it.path, "video");
                    it.description = std::to_string(cnt) + " videos";
                }
            }
            if (has_videos) {
                browser::File pa; pa.name = "Play All"; pa.type = "video_play_all"; pa.icon = "play";
                browser::File sp; sp.name = "Shuffle Play"; sp.type = "video_shuffle_play"; sp.icon = "shuffle";
                files.insert(files.begin(), sp);
                files.insert(files.begin(), pa);
            }
        }
    } else if (cat.id == "photo") {
        if (view_type == "category_root") {
            int n = 0;
            { std::lock_guard<std::mutex> l(indexing::data_mutex); n = (int)indexing::data.photos.size(); }
            browser::File c; c.name = "Photo Files"; c.type = "directory_trigger"; c.path = cat.path; c.icon = "folder_image";
            c.description = std::to_string(n) + " photos";
            files.push_back(c);
            browser::set_state(cat.path, cat.path, cat.filter);
        } else if (view_type == "browser") {
            browser::scan();
            for (auto& it : files) {
                if (it.type == "directory") {
                    int cnt = count_media_in_dir(it.path, "photo");
                    it.description = std::to_string(cnt) + " photos";
                }
            }
        }
    } else if (cat.id == "settings") {
        refresh_settings_items(false);
        return;
    } else if (cat.id == "folder") {
        if (view_type == "category_root") {
            // REG-Linux variant: single root /userdata/medias.
            browser::File s; s.name = "Primary Storage"; s.type = "directory_trigger";
            s.path = "/userdata/medias"; s.icon = "drive"; s.description = "/userdata/medias";
            files.push_back(s);
        } else if (view_type == "browser") {
            browser::scan();
        }
    } else if (!cat.path.empty()) {
        std::string base = cat.path;
        if (browser::current_dir.empty() || !utils::is_subpath(base, browser::current_dir)) {
            browser::set_state(base, base, cat.filter);
        }
        if (view_type == "browser") browser::scan();
    }
    prep_files();
}

void refresh_browser(const std::string& slide_dir) {
    close_context_menu();
    auto& cats = categories::list();
    auto& cat = cats[current_category_idx - 1];
    if (cat.id == "music" || cat.id == "video" || cat.id == "photo" || cat.id == "folder")
        view_type = "category_root";
    else
        view_type = "browser";
    view_data = nullptr;

    if ((cat.id == "music" || cat.id == "video" || cat.id == "photo") && cat.path.empty()) {
        browser::File f; f.name = "Media directory not set. Please set directory from Settings."; f.type = "info_text";
        browser::set_files({f});
    } else {
        std::string base = cat.path.empty() ? "/userdata/medias" : cat.path;
        browser::set_state(base, base, cat.filter);
        refresh_items();
    }

    current_item_idx = (cat.id == "settings") ? 2 : 1;
    item_marquee.offset = 0; item_marquee.timer = 0; item_marquee.phase = "pause_start";
    nav_stack.clear();
    target_item_scroll_y = -(current_item_idx - 1) * 75.0f;
    item_scroll_y = target_item_scroll_y;
    target_category_scroll_x = -(current_category_idx - 1) * (float)(theme::icon_size + theme::icon_spacing);
    if (slide_dir.empty()) category_scroll_x = target_category_scroll_x; // snap on first/initial
    list_slide_x = (slide_dir == "left") ? -120.0f : 120.0f;
    list_slide_alpha = 0;
    prep_files();
}

void go_back() {
    if (context_menu.active) {
        close_context_menu();
        if (settings::keytone_enabled) assets::play_sfx("nav");
        return;
    }
    int prev_idx = 1;
    if (!nav_stack.empty()) { prev_idx = nav_stack.back(); nav_stack.pop_back(); }

    auto& cats = categories::list();
    auto& cat = cats[current_category_idx - 1];

    if (cat.id == "settings") {
        settings::go_back();
        refresh_settings_items(false);
        current_item_idx = std::min(prev_idx, (int)browser::files.size());
    } else if (!cat.path.empty()) {
        if (view_type != "browser") {
            if (view_type == "album_tracks") view_type = "music_albums";
            else if (view_type == "artist_tracks") view_type = "music_artists";
            else view_type = "category_root";
            refresh_items();
            current_item_idx = std::min(prev_idx, (int)browser::files.size());
            prep_files();
        } else {
            std::string current = utils::normalize_path(browser::current_dir);
            std::string base    = utils::normalize_path(browser::base_dir);
            if ((cat.id == "music" || cat.id == "video" || cat.id == "photo" || cat.id == "folder") && current == base) {
                view_type = "category_root";
                refresh_items();
                current_item_idx = std::min(prev_idx, (int)browser::files.size());
                prep_files();
            } else if (current != base) {
                std::string parent = utils::get_dirname(browser::current_dir);
                std::string target = (!parent.empty() && utils::is_subpath(base, parent)) ? parent : base;
                browser::set_state(browser::base_dir, target, current_filter());
                refresh_items();
                current_item_idx = std::min(prev_idx, (int)browser::files.size());
                prep_files();
            }
        }
    }

    item_marquee.offset = 0; item_marquee.timer = 0; item_marquee.phase = "pause_start";
    target_item_scroll_y = -(current_item_idx - 1) * 75.0f;
    item_scroll_y = target_item_scroll_y;
    list_slide_x = -120;
    list_slide_alpha = 0;
    if (settings::keytone_enabled) assets::play_sfx("nav");
}

bool navigate(const std::string& dir) {
    auto& cats = categories::list();
    bool moved = false;

    if (settings_view::active) {
        if (settings_view::picker_active) {
            int old = settings_view::picker_selected_idx;
            if (dir == "up")   settings_view::picker_selected_idx = std::max(1, settings_view::picker_selected_idx - 1);
            if (dir == "down") settings_view::picker_selected_idx = std::min((int)settings_view::picker_items.size(), settings_view::picker_selected_idx + 1);
            settings_view::ensure_picker_visible();
            bool m = old != settings_view::picker_selected_idx;
            if (m && settings::keytone_enabled) assets::play_sfx("nav");
            return m;
        }
        int old = settings_view::selected_option_idx;
        if (dir == "up")   settings_view::selected_option_idx = std::max(1, settings_view::selected_option_idx - 1);
        if (dir == "down") {
            if (current_item_idx >= 1 && current_item_idx <= (int)browser::files.size()) {
                int sidx = browser::files[current_item_idx - 1].setting_idx;
                if (sidx >= 0 && sidx < (int)settings::options.size()) {
                    auto& opt = settings::options[sidx];
                    if (!opt.choices.empty())
                        settings_view::selected_option_idx = std::min((int)opt.choices.size(), settings_view::selected_option_idx + 1);
                }
            }
        }
        bool m = old != settings_view::selected_option_idx;
        if (m && settings::keytone_enabled) assets::play_sfx("nav");
        return m;
    }

    if (dir == "left") {
        if (in_submenu()) { go_back(); return true; }
        int old = current_category_idx;
        current_category_idx = std::max(1, current_category_idx - 1);
        if (old != current_category_idx) { refresh_browser("left"); moved = true; }
    } else if (dir == "right") {
        if (!in_submenu()) {
            int old = current_category_idx;
            current_category_idx = std::min((int)cats.size(), current_category_idx + 1);
            if (old != current_category_idx) { refresh_browser("right"); moved = true; }
        }
    } else if (dir == "down") {
        int old = current_item_idx;
        current_item_idx = std::min((int)browser::files.size(), current_item_idx + 1);
        if (old != current_item_idx) { item_marquee.offset = 0; item_marquee.timer = 0; item_marquee.phase = "pause_start"; moved = true; }
    } else if (dir == "up") {
        int old = current_item_idx;
        current_item_idx = std::max(1, current_item_idx - 1);
        if (old != current_item_idx) { item_marquee.offset = 0; item_marquee.timer = 0; item_marquee.phase = "pause_start"; moved = true; }
    }
    if (moved && settings::keytone_enabled) assets::play_sfx("nav");
    return moved;
}

void update(float dt) {
    if (context_menu.active) context_menu.alpha = std::min(1.0f, context_menu.alpha + dt * 10);
    else                     context_menu.alpha = std::max(0.0f, context_menu.alpha - dt * 10);
    if (help_overlay_active) help_overlay_alpha = std::min(1.0f, help_overlay_alpha + dt * 10);
    else                     help_overlay_alpha = std::max(0.0f, help_overlay_alpha - dt * 10);

    category_scroll_x = utils::smooth(category_scroll_x, target_category_scroll_x, dt, 10);
    item_scroll_y     = utils::smooth(item_scroll_y, target_item_scroll_y, dt, 10);
    list_slide_x      = utils::smooth(list_slide_x, 0, dt, 14);
    list_slide_alpha  = utils::smooth(list_slide_alpha, 1, dt, 10);
    if (std::fabs(list_slide_x) < 0.5f) list_slide_x = 0;
    if (list_slide_alpha > 0.99f) list_slide_alpha = 1;

    int sel_setting_idx = -1;
    if (current_item_idx >= 1 && current_item_idx <= (int)browser::files.size()) {
        sel_setting_idx = browser::files[current_item_idx - 1].setting_idx;
    }
    settings_view::update(dt, sel_setting_idx);

    target_category_scroll_x = -(current_category_idx - 1) * (float)(theme::icon_size + theme::icon_spacing);
    target_item_scroll_y     = -(current_item_idx - 1) * 75.0f;

    int cat_base_x = (int)(g_app.screen_w * 0.25f);
    item_marquee.max_width = g_app.screen_w - cat_base_x - 40;

    if (current_item_idx >= 1 && current_item_idx <= (int)browser::files.size()) {
        ui::update_marquee(item_marquee, dt, ui::text_width(assets::fonts.main, browser::files[current_item_idx - 1].name));
    } else {
        item_marquee.offset = 0; item_marquee.timer = 0; item_marquee.phase = "pause_start";
    }

    std::string current_key;
    if (gamepad::is_logical_down("up")) current_key = "up";
    else if (gamepad::is_logical_down("down")) current_key = "down";
    else if (gamepad::is_logical_down("left")) current_key = "left";
    else if (gamepad::is_logical_down("right")) current_key = "right";

    if (!current_key.empty()) {
        if (last_key == current_key) {
            repeat_timer -= dt;
            if (repeat_timer <= 0) { navigate(current_key); repeat_timer = REPEAT_INTERVAL; }
        } else {
            last_key = current_key;
            repeat_timer = REPEAT_DELAY;
        }
    } else {
        last_key.clear();
        repeat_timer = 0;
    }
}

static void enter_directory_trigger(const browser::File& selected) {
    if (selected.path.empty()) {
        ui::show_toast("Media directory not set. Please set directory from Settings.", "folder", "top_center");
        if (settings::keytone_enabled) assets::play_sfx("nav");
        return;
    }
    nav_stack.push_back(current_item_idx);
    auto& cats = categories::list();
    auto& cat = cats[current_category_idx - 1];
    if (cat.id == "folder") {
        browser::set_state(selected.path, selected.path, current_filter());
    } else {
        browser::set_state(browser::base_dir, selected.path, current_filter());
    }
    view_type = "browser";
    refresh_items();
    if (!browser::files.empty() && browser::files[0].type == "shuffle_play" && browser::files.size() > 1) current_item_idx = 2;
    else if (browser::files.size() > 2 && browser::files[0].type == "video_play_all") current_item_idx = 3;
    else current_item_idx = 1;
    prep_files();
    target_item_scroll_y = -(current_item_idx - 1) * 75.0f;
    item_scroll_y = target_item_scroll_y;
    list_slide_x = 120;
    list_slide_alpha = 0;
}

static void enter_view_trigger(const browser::File& selected) {
    nav_stack.push_back(current_item_idx);
    view_type = selected.target_view;
    refresh_items();
    current_item_idx = 1;
    prep_files();
    target_item_scroll_y = 0;
    item_scroll_y = 0;
    list_slide_x = 120;
    list_slide_alpha = 0;
}

static void enter_album_or_artist(const browser::File& selected) {
    nav_stack.push_back(current_item_idx);
    view_type = (selected.type == "album") ? "album_tracks" : "artist_tracks";
    view_data = selected.data;
    refresh_items();
    if (!browser::files.empty() && browser::files[0].type == "shuffle_play" && browser::files.size() > 1) current_item_idx = 2;
    else current_item_idx = 1;
    prep_files();
    target_item_scroll_y = -(current_item_idx - 1) * 75.0f;
    item_scroll_y = target_item_scroll_y;
    list_slide_x = 120;
    list_slide_alpha = 0;
}

static void enter_directory(const browser::File& selected) {
    nav_stack.push_back(current_item_idx);
    browser::set_state(browser::base_dir, selected.path, current_filter());
    refresh_items();
    if (!browser::files.empty() && browser::files[0].type == "shuffle_play" && browser::files.size() > 1) current_item_idx = 2;
    else if (browser::files.size() > 2 && browser::files[0].type == "video_play_all") current_item_idx = 3;
    else current_item_idx = 1;
    prep_files();
    target_item_scroll_y = -(current_item_idx - 1) * 75.0f;
    item_scroll_y = target_item_scroll_y;
    list_slide_x = 120;
    list_slide_alpha = 0;
}

static void activate_file(const browser::File& selected) {
    auto& cats = categories::list();
    auto& cat = cats[current_category_idx - 1];
    if (cat.id == "video") {
        bool is_resume = (view_type == "video_resume");
        player::play_video(selected.path, is_resume);
    } else if (cat.id == "music") {
        std::vector<music_player::Track> playlist;
        for (auto& it : browser::files) {
            if (it.type == "file") playlist.push_back({it.name, it.path});
        }
        music_player::play(selected.path, &playlist);
    } else if (cat.id == "photo") {
        image_viewer::open(selected.path, browser::files);
    } else if (cat.id == "folder") {
        if (is_in_filter(selected.path, "video")) player::play_video(selected.path);
        else if (is_in_filter(selected.path, "music")) {
            std::vector<music_player::Track> pl;
            for (auto& it : browser::files) if (it.type == "file" && is_in_filter(it.path, "music")) pl.push_back({it.name, it.path});
            music_player::play(selected.path, &pl);
        }
        else if (is_in_filter(selected.path, "photo")) image_viewer::open(selected.path, browser::files);
    }
}

static void activate_setting(const browser::File& selected) {
    if (selected.setting_idx < 0 || selected.setting_idx >= (int)settings::options.size()) return;
    auto& opt = settings::options[selected.setting_idx];
    if (opt.type == settings::OptType::Choice) {
        settings_view::active = true;
        settings_view::selected_option_idx = opt.value;
    } else if (opt.type == settings::OptType::Path) {
        std::string start = !opt.str_value.empty() ? opt.str_value : "/";
        settings_view::open_folder_picker(start, selected.setting_idx);
    } else if (opt.type == settings::OptType::Action) {
        if (opt.id == "show_controls") {
            help_overlay_active = true;
        } else if (opt.id == "clear_history") {
            history::clear();
            video_manager::clear_history();
            ui::show_toast("Watch history cleared", "history", "bottom_right");
            refresh_settings_items(false);
        } else if (opt.id == "restore_default_wallpaper") {
            if (auto* o = settings::get_option("custom_bg")) o->value = 1;
            if (auto* o = settings::get_option("custom_bg_path")) o->str_value = "assets/background/bg.jpg";
            settings::apply();
            settings::save();
            ui::show_toast("Default wallpaper restored", "theme", "bottom_right");
            refresh_settings_items(false);
        } else if (opt.id == "reindex_media") {
            settings::request_reindex_on_restart();
            g_app.quit_requested = true;
            g_app.restart_requested = true;
        } else if (opt.id == "test_toast_top") {
            ui::show_toast("Dev: Top Center Toast", "info", "top_center");
        } else if (opt.id == "test_toast_bottom") {
            ui::show_toast("Dev: Bottom Right Toast", "option", "bottom_right");
        }
    }
    if (settings::keytone_enabled) assets::play_sfx("nav");
}

static void activate_settings_group(const browser::File& selected) {
    if (selected.group_id == "quit") {
        g_app.quit_requested = true;
        return;
    }
    nav_stack.push_back(current_item_idx);
    settings::enter_group(selected.group_id);
    refresh_settings_items(false);
    current_item_idx = 1;
    item_marquee.offset = 0; item_marquee.timer = 0; item_marquee.phase = "pause_start";
    target_item_scroll_y = 0;
    item_scroll_y = 0;
    list_slide_x = 120;
    list_slide_alpha = 0;
    if (settings::keytone_enabled) assets::play_sfx("nav");
}

static void shuffle_play_handler(const browser::File& selected) {
    std::vector<music_player::Track> playlist;
    if (!selected.tracks.empty()) {
        std::lock_guard<std::mutex> l(indexing::data_mutex);
        for (auto& path : selected.tracks) {
            auto it = indexing::data.music_files.find(path);
            std::string name = (it != indexing::data.music_files.end() && !it->second.title.empty()) ? it->second.title : utils::get_filename(path);
            playlist.push_back({name, path});
        }
    } else {
        for (auto& it : browser::files) {
            if (it.type == "file" && is_in_filter(it.path, "music")) playlist.push_back({it.name, it.path});
        }
    }
    if (!playlist.empty()) {
        std::shuffle(playlist.begin(), playlist.end(), std::mt19937{std::random_device{}()});
        music_player::play(playlist[0].path, &playlist);
    }
}

static void video_play_all_handler(bool shuffle) {
    std::vector<std::string> playlist;
    for (auto& it : browser::files) if (it.type == "file") playlist.push_back(it.path);
    if (playlist.empty()) return;
    if (shuffle) std::shuffle(playlist.begin(), playlist.end(), std::mt19937{std::random_device{}()});
    player::play_video(playlist);
}

void keypressed(const std::string& key) {
    if (help_overlay_active) {
        if (key == "backspace" || key == "b" || key == "escape" || key == "x") {
            help_overlay_active = false;
            if (settings::keytone_enabled) assets::play_sfx("nav");
        }
        return;
    }
    // settings_view (choice popup) takes priority
    if (settings_view::active) {
        if (settings_view::picker_active) {
            settings_view::keypressed(key);
            return;
        }
        if (key == "up" || key == "down") { navigate(key); return; }
        if (key == "return" || key == "a" || key == "space") {
            // Apply choice
            if (current_item_idx >= 1 && current_item_idx <= (int)browser::files.size()) {
                int sidx = browser::files[current_item_idx - 1].setting_idx;
                if (sidx >= 0 && sidx < (int)settings::options.size()) {
                    settings::options[sidx].value = settings_view::selected_option_idx;
                    settings::apply();
                    settings::save();
                    refresh_settings_items(true);
                }
            }
            return;
        }
        if (key == "backspace" || key == "b" || key == "escape") { settings_view::active = false; return; }
        return;
    }

    if (context_menu.active) {
        if (key == "up") {
            context_menu.selected_idx = std::max(1, context_menu.selected_idx - 1);
            if (settings::keytone_enabled) assets::play_sfx("nav");
        } else if (key == "down") {
            context_menu.selected_idx = std::min((int)context_menu.items.size(), context_menu.selected_idx + 1);
            if (settings::keytone_enabled) assets::play_sfx("nav");
        } else if (key == "return" || key == "a" || key == "space") {
            if (context_menu.selected_idx >= 1 && context_menu.selected_idx <= (int)context_menu.items.size()) {
                apply_context_action(context_menu.items[context_menu.selected_idx - 1].id);
            }
            close_context_menu();
            if (settings::keytone_enabled) assets::play_sfx("nav");
        } else if (key == "backspace" || key == "b" || key == "escape" || key == "x") {
            close_context_menu();
            if (settings::keytone_enabled) assets::play_sfx("nav");
        }
        return;
    }

    if (key == "up" || key == "down" || key == "left" || key == "right") { navigate(key); return; }

    if (key == "return" || key == "a") {
        if (current_item_idx < 1 || current_item_idx > (int)browser::files.size()) return;
        const auto& sel = browser::files[current_item_idx - 1];
        if (sel.type == "info" || sel.type == "info_text") return;
        if      (sel.type == "directory")            enter_directory(sel);
        else if (sel.type == "file")                 activate_file(sel);
        else if (sel.type == "setting")              activate_setting(sel);
        else if (sel.type == "settings_group")       activate_settings_group(sel);
        else if (sel.type == "view_trigger")         enter_view_trigger(sel);
        else if (sel.type == "directory_trigger")    enter_directory_trigger(sel);
        else if (sel.type == "album" || sel.type == "artist") enter_album_or_artist(sel);
        else if (sel.type == "shuffle_play")         shuffle_play_handler(sel);
        else if (sel.type == "video_play_all")       video_play_all_handler(false);
        else if (sel.type == "video_shuffle_play")   video_play_all_handler(true);
        return;
    }

    if (key == "backspace" || key == "b") {
        if (in_submenu()) go_back();
        return;
    }

    if (key == "x") {
        if (current_item_idx < 1 || current_item_idx > (int)browser::files.size()) return;
        const auto& sel = browser::files[current_item_idx - 1];
        auto& cats = categories::list();
        const auto& cat = cats[current_category_idx - 1];
        if (sel.type == "file" && (cat.id == "photo" || cat.id == "folder") && is_in_filter(sel.path, "photo")) {
            open_photo_context_menu(sel.path);
            if (settings::keytone_enabled) assets::play_sfx("nav");
        } else if (sel.type == "file" && (cat.id == "video" || cat.id == "folder")) {
            open_video_context_menu(sel.path);
            if (settings::keytone_enabled) assets::play_sfx("nav");
        }
    }
}

} // namespace xmb
