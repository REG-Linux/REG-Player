#include "history.h"
#include "persist.h"
#include "utils.h"
#include <nlohmann/json.hpp>

namespace history {

std::vector<std::string> data;

static std::string file_path() { return utils::config_dir() + "/history.json"; }

void load() {
    nlohmann::json j;
    if (!persist::read_json(file_path(), j)) return;
    if (!j.is_array()) return;
    data.clear();
    for (auto& v : j) if (v.is_string()) data.push_back(v.get<std::string>());
}

void save() {
    nlohmann::json j = data;
    persist::write_json(file_path(), j);
}

void add(const std::string& path) {
    if (path.empty()) return;
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == path) { data.erase(data.begin() + i); break; }
    }
    data.insert(data.begin(), path);
    while (data.size() > 50) data.pop_back();
    save();
}

void clear() {
    data.clear();
    save();
}

bool prune_missing(const std::unordered_set<std::string>& valid) {
    std::vector<std::string> kept;
    bool changed = false;
    for (auto& p : data) {
        if (valid.count(p)) kept.push_back(p);
        else changed = true;
    }
    if (changed) { data = std::move(kept); save(); }
    return changed;
}

} // namespace history
