#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace video_manager {

struct ResumeEntry {
    std::string name;
    std::string path;
};

extern std::unordered_map<std::string, bool> watched_data;

void load_watched();
void save_watched();
bool is_watched(const std::string& path);
bool set_watched(const std::string& path, bool watched);
bool toggle_watched(const std::string& path);
void clear_resume(const std::string& path);
bool prune_stale_state(const std::unordered_set<std::string>& valid_paths);
void clear_history();
std::vector<ResumeEntry> get_resume_list();

std::string watch_later_dir();

} // namespace video_manager
