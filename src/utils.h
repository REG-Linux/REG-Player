#pragma once
#include <string>
#include <vector>
#include <SDL3/SDL.h>

namespace utils {

std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char sep);
float lerp(float a, float b, float t);
float smooth(float a, float b, float dt, float speed);
std::string format_time(double seconds); // "M:SS"

std::string get_filename(const std::string& path);
std::string get_extension(const std::string& path);   // ".mp3" lowercase, or ""
std::string get_dirname(const std::string& path);
std::string normalize_path(const std::string& p);
bool is_subpath(const std::string& base, const std::string& current);
std::string get_track_name(const std::string& filename); // strip ext

std::string clean_utf8(const std::string& s);

// truncate using TTF font; returns string that fits in max_w with "..." appended if shrunk
struct TTF_FontHandle; // opaque
std::string truncate_text(const std::string& text, void* font /*TTF_Font*/, int max_w);

void shuffle(std::vector<std::string>& v);
template <class T>
void shuffle_vec(std::vector<T>& v);

std::string config_dir();   // ~/.config/regplayer
std::string cache_dir();    // ~/.cache/regplayer
std::string thumb_dir();    // <cache>/thumbnails

} // namespace utils
