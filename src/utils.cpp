#include "utils.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>

namespace utils {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(sep, i);
        if (j == std::string::npos) j = s.size();
        out.push_back(trim(s.substr(i, j - i)));
        i = j + 1;
    }
    if (s.empty()) out.clear();
    return out;
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

float smooth(float a, float b, float dt, float speed) {
    if (a == b) return a;
    float t = 1.0f - std::exp(-speed * dt);
    return a + (b - a) * t;
}

std::string format_time(double seconds) {
    if (seconds < 0 || std::isnan(seconds)) seconds = 0;
    int total = (int)seconds;
    int mins = total / 60;
    int secs = total % 60;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
    return buf;
}

std::string get_filename(const std::string& path) {
    size_t p = path.find_last_of('/');
    return p == std::string::npos ? path : path.substr(p + 1);
}

std::string get_extension(const std::string& path) {
    size_t p = path.find_last_of('.');
    if (p == std::string::npos) return "";
    if (p < path.find_last_of('/') && path.find_last_of('/') != std::string::npos) return "";
    std::string ext = path.substr(p);
    for (auto& c : ext) c = std::tolower((unsigned char)c);
    return ext;
}

std::string get_dirname(const std::string& path) {
    size_t p = path.find_last_of('/');
    if (p == std::string::npos) return "";
    return path.substr(0, p);
}

std::string normalize_path(const std::string& p) {
    std::string s = p;
    for (auto& c : s) if (c == '\\') c = '/';
    std::string out;
    out.reserve(s.size());
    bool prev_slash = false;
    for (char c : s) {
        if (c == '/') {
            if (!prev_slash) out.push_back(c);
            prev_slash = true;
        } else {
            out.push_back(c);
            prev_slash = false;
        }
    }
    if (out.size() > 1 && out.back() == '/') out.pop_back();
    return out;
}

bool is_subpath(const std::string& base_, const std::string& current_) {
    std::string base = normalize_path(base_);
    std::string current = normalize_path(current_);
    if (base == current) return true;
    if (base == "/") return !current.empty() && current[0] == '/';
    if (current.size() < base.size() + 1) return false;
    return current.compare(0, base.size(), base) == 0 && current[base.size()] == '/';
}

std::string get_track_name(const std::string& filename) {
    std::string name = get_filename(filename);
    size_t p = name.find_last_of('.');
    if (p == std::string::npos) return name;
    return name.substr(0, p);
}

std::string clean_utf8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char b = (unsigned char)s[i];
        if (b < 128) { out.push_back((char)b); ++i; continue; }
        if (b >= 192 && b <= 223) {
            if (i + 1 < s.size()) {
                unsigned char b2 = (unsigned char)s[i + 1];
                if (b2 >= 128 && b2 <= 191) { out.append(s, i, 2); i += 2; continue; }
            }
            out.push_back('?'); ++i; continue;
        }
        if (b >= 224 && b <= 239) {
            if (i + 2 < s.size()) {
                unsigned char b2 = (unsigned char)s[i + 1], b3 = (unsigned char)s[i + 2];
                if (b2 >= 128 && b2 <= 191 && b3 >= 128 && b3 <= 191) { out.append(s, i, 3); i += 3; continue; }
            }
            out.push_back('?'); ++i; continue;
        }
        if (b >= 240 && b <= 247) {
            if (i + 3 < s.size()) {
                unsigned char b2 = (unsigned char)s[i + 1], b3 = (unsigned char)s[i + 2], b4 = (unsigned char)s[i + 3];
                if (b2 >= 128 && b2 <= 191 && b3 >= 128 && b3 <= 191 && b4 >= 128 && b4 <= 191) {
                    out.append(s, i, 4); i += 4; continue;
                }
            }
            out.push_back('?'); ++i; continue;
        }
        out.push_back('?'); ++i;
    }
    return out;
}

// step over one UTF-8 codepoint starting at position i; returns next-byte position.
static size_t utf8_next(const std::string& s, size_t i) {
    if (i >= s.size()) return i;
    unsigned char b = (unsigned char)s[i];
    if (b < 0x80) return i + 1;
    if ((b & 0xE0) == 0xC0) return i + 2;
    if ((b & 0xF0) == 0xE0) return i + 3;
    if ((b & 0xF8) == 0xF0) return i + 4;
    return i + 1;
}

static int text_width(TTF_Font* font, const std::string& s) {
    int w = 0, h = 0;
    if (TTF_GetStringSize(font, s.c_str(), s.size(), &w, &h)) return w;
    return 0;
}

std::string truncate_text(const std::string& text_in, void* font_, int max_w) {
    TTF_Font* font = (TTF_Font*)font_;
    std::string text = clean_utf8(text_in);
    if (text_width(font, text) <= max_w) return text;

    size_t last_pos = 0;
    size_t i = 0;
    while (i < text.size()) {
        size_t next_pos = utf8_next(text, i);
        std::string part = text.substr(0, next_pos) + "...";
        if (text_width(font, part) > max_w) {
            if (last_pos == 0) return "...";
            return text.substr(0, last_pos) + "...";
        }
        last_pos = next_pos;
        i = next_pos;
    }
    return "...";
}

void shuffle(std::vector<std::string>& v) {
    static std::mt19937 rng{std::random_device{}()};
    std::shuffle(v.begin(), v.end(), rng);
}

// generic
template <class T>
void shuffle_vec(std::vector<T>& v) {
    static std::mt19937 rng{std::random_device{}()};
    std::shuffle(v.begin(), v.end(), rng);
}

template void shuffle_vec<std::string>(std::vector<std::string>&);

static std::string env_or(const char* var, const std::string& fallback) {
    const char* v = std::getenv(var);
    return (v && *v) ? std::string(v) : fallback;
}

static void mkpath(const std::string& path) {
    std::string p;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/' && i > 0) {
            mkdir(p.c_str(), 0755);
        }
        p.push_back(path[i]);
    }
    mkdir(p.c_str(), 0755);
}

std::string config_dir() {
    std::string base = env_or("XDG_CONFIG_HOME", env_or("HOME", "/tmp") + "/.config");
    std::string d = base + "/regplayer";
    mkpath(d);
    return d;
}

std::string cache_dir() {
    std::string base = env_or("XDG_CACHE_HOME", env_or("HOME", "/tmp") + "/.cache");
    std::string d = base + "/regplayer";
    mkpath(d);
    return d;
}

std::string thumb_dir() {
    std::string d = cache_dir() + "/thumbnails";
    mkpath(d);
    return d;
}

} // namespace utils
