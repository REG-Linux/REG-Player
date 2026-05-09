// Port of settings.lua. Bug fixed: in original, force_reindex_once block was
// nested inside vol_bright_control's table (settings.lua:154-160). Separated here.
#include "settings.h"
#include "categories.h"
#include "persist.h"
#include "theme.h"
#include "utils.h"
#include <cstdio>
#include <nlohmann/json.hpp>

namespace settings {

bool keytone_enabled = true;
bool vol_bright_enabled = true;
bool show_particles = true;
int  display_sleep_seconds = 0;

std::vector<Group> groups = {
    {"quit",            "Quit REG-Player",      "quit"},
    {"general",         "General Settings",   "option"},
    {"theme_settings",  "Theme Settings",     "theme"},
    {"media_dirs",      "Media Directories",  "folder"},
    {"about",           "About REG-Player",     "info"},
    {"devtools",        "Dev Tools",          "option"},
};

std::vector<Option> options = {
    {"theme",                       "Theme",                          OptType::Choice, "theme_settings", {"Light", "Dark"}, 1, "", ""},
    {"theme_color",                 "Theme Color",                    OptType::Choice, "theme_settings",
        {"Dark Red","Volcanic","Golden","Lime Green","Apple Green","Moss Green","Undersea","Electric Blue","Midnight Blue","Dark Purple","Ice Cold","Morning Blue","Gray Light","Gray Dark"}, 8, "", ""},
    {"particles",                   "Particle Effects",               OptType::Choice, "theme_settings", {"Show","Hide"}, 1, "", ""},
    {"custom_bg",                   "Wallpaper",                      OptType::Choice, "theme_settings", {"On","Off"}, 2, "", ""},
    {"custom_bg_path",              "Wallpaper Path",                 OptType::Path,   "internal", {}, 0, "", ""},
    {"blur_wallpaper",              "Blur Wallpaper",                 OptType::Choice, "theme_settings", {"No","Yes"}, 1, "", ""},
    {"tint_wallpaper",              "Tint Wallpaper to Theme Color",  OptType::Choice, "theme_settings", {"No","Yes"}, 1, "", ""},
    {"wallpaper_brightness",        "Wallpaper Brightness",           OptType::Choice, "theme_settings", {"No Change","Brighter","Darker"}, 1, "", ""},
    {"photo_dir",                   "Photo Directory",                OptType::Path,   "media_dirs", {}, 0, "/userdata/medias/photos", ""},
    {"music_dir",                   "Music Directory",                OptType::Path,   "media_dirs", {}, 0, "/userdata/medias/music",  ""},
    {"video_dir",                   "Video Directory",                OptType::Path,   "media_dirs", {}, 0, "/userdata/medias/videos", ""},
    {"reindex_media",               "Reindex Media and Restart App",  OptType::Action, "media_dirs", {}, 0, "", "repeat"},
    {"keytone",                     "Key Tone",                       OptType::Choice, "general", {"On","Off"}, 1, "", ""},
    {"vol_bright_control",          "Volume & Brightness Control",    OptType::Choice, "general", {"Show","Hide"}, 1, "", ""},
    {"force_reindex_once",          "Force Reindex Once",             OptType::Flag,   "internal", {}, 0, "", ""},
    {"display_sleep",               "Auto Display Sleep (Music)",     OptType::Choice, "general", {"Off","5s","10s","15s","30s","1m","3m"}, 1, "", ""},
    {"show_controls",               "Show Controls",                  OptType::Action, "general", {}, 0, "", ""},
    {"clear_history",               "Clear Watch History",            OptType::Action, "general", {}, 0, "", ""},
    {"restore_default_wallpaper",   "Restore Default Wallpaper",      OptType::Action, "general", {}, 0, "", ""},
    {"version",                     "Version",                        OptType::Info,   "about",   {}, 0, "v0.1", ""},
    {"website",                     "Website",                        OptType::Info,   "about",   {}, 0, "github.com/REG-Linux/REG-Player", ""},
    {"test_toast_top",              "Test Top Center Toast",          OptType::Action, "devtools",{}, 0, "", ""},
    {"test_toast_bottom",           "Test Bottom Right Toast",        OptType::Action, "devtools",{}, 0, "", ""},
};

std::string current_group;

Option* get_option(const std::string& id) {
    for (auto& o : options) if (o.id == id) return &o;
    return nullptr;
}

static std::string config_file() { return utils::config_dir() + "/settings.json"; }

void save() {
    nlohmann::json j;
    for (auto& o : options) {
        if (o.type == OptType::Path || o.type == OptType::Info) {
            j[o.id] = o.str_value;
        } else {
            j[o.id] = o.value;
        }
    }
    persist::write_json(config_file(), j);
}

void load() {
    nlohmann::json j;
    if (persist::read_json(config_file(), j)) {
        for (auto& o : options) {
            if (!j.contains(o.id)) continue;
            const auto& v = j[o.id];
            if (o.type == OptType::Path || o.type == OptType::Info) {
                if (v.is_string()) o.str_value = v.get<std::string>();
            } else {
                if (v.is_number_integer()) o.value = v.get<int>();
            }
        }
    }
    apply();
}

void apply() {
    Option* opt_theme = get_option("theme");
    Option* opt_color = get_option("theme_color");
    if (opt_theme && opt_color) {
        const std::string& mode = opt_theme->choices[opt_theme->value - 1];
        const std::string& cn   = opt_color->choices[opt_color->value - 1];
        theme::apply(mode, cn);
    }

    auto set_cat_path = [](const char* cat_id, Option* opt) {
        if (!opt) return;
        for (auto& c : categories::list()) {
            if (c.id == cat_id) {
                if (!opt->str_value.empty()) c.path = opt->str_value;
                break;
            }
        }
    };
    set_cat_path("photo", get_option("photo_dir"));
    set_cat_path("music", get_option("music_dir"));
    set_cat_path("video", get_option("video_dir"));

    if (auto* o = get_option("keytone"))            keytone_enabled    = (o->value == 1);
    if (auto* o = get_option("vol_bright_control")) vol_bright_enabled = (o->value == 1);
    if (auto* o = get_option("particles"))          show_particles     = (o->value == 1);

    if (auto* o = get_option("display_sleep")) {
        static const int choices[] = {0, 5, 10, 15, 30, 60, 180};
        int idx = std::max(1, std::min((int)(sizeof(choices)/sizeof(choices[0])), o->value)) - 1;
        display_sleep_seconds = choices[idx];
    } else {
        display_sleep_seconds = 0;
    }

    // Background hooks deferred — handled when background module ready.
}

void request_reindex_on_restart() {
    if (auto* o = get_option("force_reindex_once")) { o->value = 1; save(); }
}

bool consume_reindex_request() {
    if (auto* o = get_option("force_reindex_once")) {
        if (o->value == 1) { o->value = 0; save(); return true; }
    }
    return false;
}

bool in_submenu() { return !current_group.empty(); }
void go_back() { current_group.clear(); }
void enter_group(const std::string& id) { current_group = id; }

std::vector<BrowserItem> get_browser_items() {
    std::vector<BrowserItem> items;
    if (!current_group.empty()) {
        for (size_t i = 0; i < options.size(); ++i) {
            const auto& o = options[i];
            if (o.group != current_group) continue;
            std::string display_value;
            if (o.type == OptType::Choice) {
                if (o.value >= 1 && (size_t)o.value <= o.choices.size())
                    display_value = ": " + o.choices[o.value - 1];
            } else if (o.type == OptType::Path || o.type == OptType::Info) {
                display_value = ": " + o.str_value;
            }
            std::string icon = "folder";
            if (o.group == "about") icon = "info";
            else if (o.group == "general" || o.group == "theme_settings" || o.group == "devtools") icon = "option";

            BrowserItem it;
            it.name = o.name + display_value;
            it.type = (o.type == OptType::Info) ? "info_text" : "setting";
            it.setting_idx = (int)i;
            it.icon = icon;
            items.push_back(it);
        }
    } else {
        for (size_t i = 0; i < groups.size(); ++i) {
            BrowserItem it;
            it.name = groups[i].name;
            it.type = "settings_group";
            it.group_idx = (int)i;
            it.group_id = groups[i].id;
            it.icon = groups[i].icon;
            items.push_back(it);
        }
    }
    return items;
}

} // namespace settings
