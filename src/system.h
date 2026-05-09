#pragma once
#include <optional>

namespace system_info {

std::optional<int> get_battery_percentage();
bool is_charging();
std::optional<int> get_volume();      // 0..100
std::optional<int> get_brightness();  // 0..max

} // namespace system_info
