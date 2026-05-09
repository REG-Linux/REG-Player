#pragma once
#include <string>
#include <vector>

namespace settings {

enum class OptType { Choice, Path, Action, Info, Flag };

struct Option {
    std::string id;
    std::string name;
    OptType type = OptType::Choice;
    std::string group;
    std::vector<std::string> choices;
    int value = 0;
    std::string str_value;
    std::string icon;
};

struct Group {
    std::string id;
    std::string name;
    std::string icon;
};

extern bool keytone_enabled;
extern bool vol_bright_enabled;
extern bool show_particles;
extern int  display_sleep_seconds;

extern std::vector<Option> options;
extern std::vector<Group> groups;

extern std::string current_group; // empty = top-level

void load();
void save();
void apply();

Option* get_option(const std::string& id);

void request_reindex_on_restart();
bool consume_reindex_request();

bool in_submenu();
void go_back();
void enter_group(const std::string& id);

struct BrowserItem {
    std::string name;
    std::string type; // "settings_group" | "setting" | "info_text"
    int setting_idx = -1;
    int group_idx = -1;
    std::string group_id;
    std::string icon;
};

std::vector<BrowserItem> get_browser_items();

} // namespace settings
