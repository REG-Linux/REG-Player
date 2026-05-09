#include "video_manager.h"
#include "persist.h"
#include "utils.h"
#include <cstdio>
#include <dirent.h>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <unistd.h>

namespace video_manager {

std::unordered_map<std::string, bool> watched_data;

static std::string watched_file() { return utils::config_dir() + "/watched.json"; }

std::string watch_later_dir() {
    std::string d = utils::config_dir() + "/mpv/watch_later";
    // mkdir -p
    std::string p;
    for (size_t i = 0; i < d.size(); ++i) {
        if (d[i] == '/' && i > 0) mkdir(p.c_str(), 0755);
        p.push_back(d[i]);
    }
    mkdir(p.c_str(), 0755);
    return d;
}

void load_watched() {
    nlohmann::json j;
    if (!persist::read_json(watched_file(), j)) return;
    if (!j.is_object()) return;
    watched_data.clear();
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.value().is_boolean() && it.value().get<bool>()) watched_data[it.key()] = true;
    }
}

void save_watched() {
    nlohmann::json j = nlohmann::json::object();
    for (auto& kv : watched_data) j[kv.first] = true;
    persist::write_json(watched_file(), j);
}

bool is_watched(const std::string& path) {
    auto it = watched_data.find(path);
    return it != watched_data.end() && it->second;
}

bool set_watched(const std::string& path, bool w) {
    if (w) {
        if (!is_watched(path)) {
            watched_data[path] = true;
            clear_resume(path);
        }
    } else {
        watched_data.erase(path);
    }
    save_watched();
    return is_watched(path);
}

bool toggle_watched(const std::string& path) {
    return set_watched(path, !is_watched(path));
}

static std::string read_resume_path(const std::string& full_watch_path) {
    std::string content;
    if (!persist::read_file(full_watch_path, content)) return "";
    // Look for "# path: ..."
    size_t p = content.find("# path: ");
    if (p == std::string::npos) p = content.find("#path: ");
    if (p == std::string::npos) {
        // Fallback "# /..." absolute path on first comment line
        size_t hash = content.find("# /");
        if (hash != std::string::npos) {
            size_t eol = content.find('\n', hash);
            std::string s = content.substr(hash + 2, (eol == std::string::npos ? content.size() : eol) - (hash + 2));
            return utils::trim(s);
        }
        return "";
    }
    size_t start = content.find(": ", p);
    if (start == std::string::npos) return "";
    start += 2;
    size_t eol = content.find('\n', start);
    std::string s = content.substr(start, (eol == std::string::npos ? content.size() : eol) - start);
    return utils::trim(s);
}

void clear_resume(const std::string& path) {
    if (path.empty()) return;
    std::string dir = watch_later_dir();
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    while (auto* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        std::string full = dir + "/" + e->d_name;
        if (read_resume_path(full) == path) {
            ::unlink(full.c_str());
            break;
        }
    }
    closedir(d);
}

bool prune_stale_state(const std::unordered_set<std::string>& valid) {
    bool changed = false;
    for (auto it = watched_data.begin(); it != watched_data.end(); ) {
        if (!valid.count(it->first)) { it = watched_data.erase(it); changed = true; }
        else ++it;
    }
    if (changed) save_watched();

    std::string dir = watch_later_dir();
    DIR* d = opendir(dir.c_str());
    if (!d) return changed;
    while (auto* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        std::string full = dir + "/" + e->d_name;
        std::string p = read_resume_path(full);
        if (p.empty() || !valid.count(p)) {
            ::unlink(full.c_str());
            changed = true;
        }
    }
    closedir(d);
    return changed;
}

void clear_history() {
    watched_data.clear();
    save_watched();
    std::string dir = watch_later_dir();
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    while (auto* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        ::unlink((dir + "/" + e->d_name).c_str());
    }
    closedir(d);
}

std::vector<ResumeEntry> get_resume_list() {
    std::vector<ResumeEntry> out;
    std::string dir = watch_later_dir();
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    while (auto* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        std::string full = dir + "/" + e->d_name;
        std::string p = read_resume_path(full);
        if (p.empty()) continue;
        struct stat st;
        if (::stat(p.c_str(), &st) != 0) {
            ::unlink(full.c_str());
            continue;
        }
        out.push_back({utils::get_filename(p), p});
    }
    closedir(d);
    return out;
}

} // namespace video_manager
