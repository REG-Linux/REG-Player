#pragma once
#include <string>
#include <vector>

namespace player {

extern bool needs_refresh;

void play_video(const std::string& filepath, bool resume = false);
void play_video(const std::vector<std::string>& paths, bool resume = false);

} // namespace player
