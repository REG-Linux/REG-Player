#pragma once
#include <string>
#include <vector>
#include <SDL3/SDL.h>

namespace browser {

struct File {
    std::string name;
    std::string path;
    std::string type; // "directory" | "file" | "info" | "info_text" | "view_trigger" | "directory_trigger" | "album" | "artist" | "shuffle_play" | "video_play_all" | "video_shuffle_play" | "settings_group" | "setting"
    std::string display_name;
    std::string description;
    std::string icon;
    std::string target_view;
    int setting_idx = -1;
    int group_idx = -1;
    std::string group_id;
    std::vector<std::string> tracks; // for shuffle_play from indexed tracks
    void* data = nullptr; // album/artist data ptr
};

extern std::string base_dir;
extern std::string current_dir;
extern std::vector<File> files;
extern std::string filter; // "music"|"photo"|"video"|""

void set_filter(const std::string& type);
void scan(const std::string& path = "");
void set_state(const std::string& base, const std::string& current, const std::string& filter);
void set_files(std::vector<File> list);

} // namespace browser
