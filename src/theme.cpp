#include "theme.h"
#include <unordered_map>
#include <vector>

namespace theme {

std::string current_mode = "Light";

Color bg{0.05f, 0.05f, 0.08f, 1};
Color text{0.95f, 0.95f, 1.0f, 1};
Color text_dim{0.95f, 0.95f, 1.0f, 0.6f};
Color accent{0.2f, 0.4f, 0.8f, 1};

Colors colors{};

static const std::unordered_map<std::string, Color>& accents() {
    static const std::unordered_map<std::string, Color> a = {
        {"Electric Blue",  {0.0f, 0.6f, 1.0f}},
        {"Apple Green",    {0.5f, 0.9f, 0.1f}},
        {"Undersea",       {0.0f, 0.4f, 0.5f}},
        {"Volcanic",       {0.9f, 0.3f, 0.1f}},
        {"Dark Red",       {0.6f, 0.0f, 0.0f}},
        {"Dark Purple",    {0.4f, 0.1f, 0.6f}},
        {"Moss Green",     {0.4f, 0.5f, 0.2f}},
        {"Golden",         {1.0f, 0.8f, 0.2f}},
        {"Midnight Blue",  {0.1f, 0.2f, 0.4f}},
        {"Morning Blue",   {0.6f, 0.8f, 0.9f}},
        {"Lime Green",     {0.8f, 1.0f, 0.0f}},
        {"Ice Cold",       {0.7f, 0.9f, 1.0f}},
        {"Gray Dark",      {0.3f, 0.3f, 0.3f}},
        {"Gray Light",     {0.7f, 0.7f, 0.7f}},
    };
    return a;
}

struct Mode {
    Color background;
    Color text;
    Color text_dim;
};

static const std::unordered_map<std::string, Mode>& modes() {
    static const std::unordered_map<std::string, Mode> m = {
        {"Light", {{0.5f, 0.5f, 0.6f}, {0.95f, 0.95f, 1.0f}, {0.95f, 0.95f, 1.0f, 0.6f}}},
        {"Dark",  {{0.02f, 0.02f, 0.04f}, {0.95f, 0.95f, 1.0f}, {0.95f, 0.95f, 1.0f, 0.6f}}},
    };
    return m;
}

const std::vector<std::string>& accent_names() {
    static const std::vector<std::string> v = {
        "Dark Red", "Volcanic", "Golden", "Lime Green", "Apple Green", "Moss Green",
        "Undersea", "Electric Blue", "Midnight Blue", "Dark Purple", "Ice Cold",
        "Morning Blue", "Gray Light", "Gray Dark"
    };
    return v;
}

void apply(const std::string& mode, const std::string& color_name) {
    current_mode = mode;
    auto mit = modes().find(mode);
    const Mode& m = (mit != modes().end()) ? mit->second : modes().at("Light");
    auto cit = accents().find(color_name);
    const Color& c = (cit != accents().end()) ? cit->second : accents().at("Electric Blue");

    Color bg_tinted;
    if (mode == "Light") {
        const float color_weight = 0.5f;
        bg_tinted = {c.r * color_weight, c.g * color_weight, c.b * color_weight};
    } else {
        const float color_weight = 0.12f;
        bg_tinted = {
            m.background.r + c.r * color_weight,
            m.background.g + c.g * color_weight,
            m.background.b + c.b * color_weight
        };
    }

    colors.background = bg_tinted;
    colors.text = m.text;
    text = {m.text.r, m.text.g, m.text.b, 1};
    text_dim = m.text_dim;
    accent = {c.r, c.g, c.b, 1};
    bg = {bg_tinted.r, bg_tinted.g, bg_tinted.b, 1};
}

} // namespace theme
