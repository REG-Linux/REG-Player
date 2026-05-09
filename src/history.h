#pragma once
#include <string>
#include <unordered_set>
#include <vector>

namespace history {

extern std::vector<std::string> data;

void load();
void save();
void add(const std::string& path);
void clear();
bool prune_missing(const std::unordered_set<std::string>& valid_paths);

} // namespace history
