#pragma once
#include "common.h"
#include <string>
#include <unordered_map>

namespace theme {

constexpr int font_size_main = 24;
constexpr int font_size_small = 18;
constexpr int icon_size = 64;
constexpr int icon_spacing = 64;
constexpr float glow_intensity = 0.3f;
constexpr int glow_radius = 7;
constexpr float shadow_intensity = 0.5f;

extern std::string current_mode;

extern Color bg;
extern Color text;
extern Color text_dim;
extern Color accent;

struct Colors {
    Color background{0.05f, 0.05f, 0.08f};
    Color text{0.95f, 0.95f, 1.0f};
    Color shadow{0.0f, 0.0f, 0.0f, 0.15f};
    Color highlight{0.0f, 0.0f, 0.0f, 0.1f};
};
extern Colors colors;

void apply(const std::string& mode, const std::string& color_name);

const std::vector<std::string>& accent_names();

} // namespace theme
