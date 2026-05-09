#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace indexing {

struct MusicFileInfo {
    std::string title;
    std::string artist;
    std::string album;
    std::string album_artist;
    int track_number = -1;
    int disc_number = -1;
};

struct AlbumEntry {
    std::string name;
    std::string artist;
    std::vector<std::string> tracks;
    std::string thumb_path;
};

struct ArtistEntry {
    std::string name;
    std::vector<std::string> albums;
    std::vector<std::string> tracks;
};

struct PhotoInfo {
    std::string thumb_path;
};

struct Data {
    int version = 2;
    std::unordered_map<std::string, MusicFileInfo> music_files;
    std::unordered_map<std::string, AlbumEntry>    music_albums;   // key = "name::group"
    std::unordered_map<std::string, ArtistEntry>   music_artists;
    std::unordered_map<std::string, PhotoInfo>     photos;
    std::vector<std::string>                       videos;
};

extern Data data;
extern std::atomic<bool> is_scanning;
extern std::atomic<bool> cancel_scan;
extern std::mutex data_mutex;

const std::vector<std::string>& compatible_extensions(const std::string& kind);

bool load();
void save();
std::string current_progress();

void scan_async(const std::string& photo_dir, const std::string& music_dir, const std::string& video_dir);
void scan_for_new_async(const std::string& photo_dir, const std::string& music_dir, const std::string& video_dir);
void wait_finish();

} // namespace indexing
