// Direct port of indexing.lua scan flow + thumbnail generation via stb.
#include "indexing.h"
#include "chiptune.h"
#include "metadata.h"
#include "persist.h"
#include "utils.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <climits>
#include <cstring>
#include <ftw.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_write.h>
#include <thread>
#include <unordered_set>

namespace indexing {

Data data;
std::atomic<bool> is_scanning{false};
std::atomic<bool> cancel_scan{false};
std::mutex data_mutex;
static std::atomic<std::string*> g_progress_ptr{nullptr};
static std::string g_progress_str = "";
static std::mutex g_progress_mutex;
static std::thread g_thread;

static const std::vector<std::string> EXT_MUSIC = {
    ".mp3", ".wav", ".flac", ".ogg", ".m4a",
    // Trackers (libxmp)
    ".mod", ".xm", ".it", ".s3m", ".mtm", ".stm", ".669", ".far", ".ult",
    ".ams", ".med", ".okt", ".dbm", ".liq", ".mdl", ".mt2", ".pt36",
    ".ptm", ".rtm", ".gdm", ".imf", ".dsm", ".psm",
    // Chiptune (libgme)
    ".nsf", ".nsfe", ".gbs", ".spc", ".vgm", ".vgz",
    ".gym", ".ay", ".hes", ".sap", ".kss",
    // Atari ST (StSound)
    ".ym", ".sndh",
    // Commodore 64 (cSID-light / TinySID)
    ".sid"
};
static const std::vector<std::string> EXT_PHOTO = {".jpg", ".jpeg", ".png", ".gif", ".bmp"};
static const std::vector<std::string> EXT_VIDEO = {".mp4", ".mkv", ".avi", ".mov", ".wmv"};

const std::vector<std::string>& compatible_extensions(const std::string& kind) {
    static const std::vector<std::string> empty;
    if (kind == "music") return EXT_MUSIC;
    if (kind == "photo") return EXT_PHOTO;
    if (kind == "video") return EXT_VIDEO;
    return empty;
}

static void set_progress(const std::string& s) {
    std::lock_guard<std::mutex> lock(g_progress_mutex);
    g_progress_str = s;
}

std::string current_progress() {
    std::lock_guard<std::mutex> lock(g_progress_mutex);
    return g_progress_str;
}

static std::string index_file() { return utils::config_dir() + "/index.json"; }

bool load() {
    nlohmann::json j;
    if (!persist::read_json(index_file(), j)) return false;
    if (!j.contains("version") || j["version"].get<int>() != 2) return false;
    std::lock_guard<std::mutex> lock(data_mutex);
    data = Data{};
    if (j.contains("videos") && j["videos"].is_array()) {
        for (auto& v : j["videos"]) data.videos.push_back(v.get<std::string>());
    }
    if (j.contains("music_files") && j["music_files"].is_object()) {
        for (auto it = j["music_files"].begin(); it != j["music_files"].end(); ++it) {
            MusicFileInfo m;
            const auto& v = it.value();
            if (v.contains("title")) m.title = v["title"].get<std::string>();
            if (v.contains("artist")) m.artist = v["artist"].get<std::string>();
            if (v.contains("album")) m.album = v["album"].get<std::string>();
            if (v.contains("album_artist")) m.album_artist = v["album_artist"].get<std::string>();
            if (v.contains("track_number")) m.track_number = v["track_number"].get<int>();
            if (v.contains("disc_number")) m.disc_number = v["disc_number"].get<int>();
            data.music_files[it.key()] = m;
        }
    }
    if (j.contains("photos") && j["photos"].is_object()) {
        for (auto it = j["photos"].begin(); it != j["photos"].end(); ++it) {
            PhotoInfo p;
            if (it.value().contains("thumb_path")) p.thumb_path = it.value()["thumb_path"].get<std::string>();
            data.photos[it.key()] = p;
        }
    }
    if (j.contains("music_albums") && j["music_albums"].is_object()) {
        for (auto it = j["music_albums"].begin(); it != j["music_albums"].end(); ++it) {
            AlbumEntry a;
            const auto& v = it.value();
            if (v.contains("name")) a.name = v["name"].get<std::string>();
            if (v.contains("artist")) a.artist = v["artist"].get<std::string>();
            if (v.contains("thumb_path")) a.thumb_path = v["thumb_path"].get<std::string>();
            if (v.contains("tracks") && v["tracks"].is_array())
                for (auto& t : v["tracks"]) a.tracks.push_back(t.get<std::string>());
            data.music_albums[it.key()] = a;
        }
    }
    if (j.contains("music_artists") && j["music_artists"].is_object()) {
        for (auto it = j["music_artists"].begin(); it != j["music_artists"].end(); ++it) {
            ArtistEntry ar;
            const auto& v = it.value();
            if (v.contains("name")) ar.name = v["name"].get<std::string>();
            if (v.contains("albums") && v["albums"].is_array())
                for (auto& t : v["albums"]) ar.albums.push_back(t.get<std::string>());
            if (v.contains("tracks") && v["tracks"].is_array())
                for (auto& t : v["tracks"]) ar.tracks.push_back(t.get<std::string>());
            data.music_artists[it.key()] = ar;
        }
    }
    return true;
}

void save() {
    std::lock_guard<std::mutex> lock(data_mutex);
    nlohmann::json j;
    j["version"] = 2;
    nlohmann::json mf = nlohmann::json::object();
    for (auto& kv : data.music_files) {
        nlohmann::json m;
        m["title"] = kv.second.title;
        m["artist"] = kv.second.artist;
        m["album"] = kv.second.album;
        m["album_artist"] = kv.second.album_artist;
        m["track_number"] = kv.second.track_number;
        m["disc_number"] = kv.second.disc_number;
        mf[kv.first] = m;
    }
    j["music_files"] = mf;
    nlohmann::json ph = nlohmann::json::object();
    for (auto& kv : data.photos) {
        nlohmann::json p;
        p["thumb_path"] = kv.second.thumb_path;
        ph[kv.first] = p;
    }
    j["photos"] = ph;
    j["videos"] = data.videos;

    nlohmann::json al = nlohmann::json::object();
    for (auto& kv : data.music_albums) {
        nlohmann::json a;
        a["name"] = kv.second.name;
        a["artist"] = kv.second.artist;
        a["thumb_path"] = kv.second.thumb_path;
        a["tracks"] = kv.second.tracks;
        al[kv.first] = a;
    }
    j["music_albums"] = al;
    nlohmann::json ar = nlohmann::json::object();
    for (auto& kv : data.music_artists) {
        nlohmann::json a;
        a["name"] = kv.second.name;
        a["albums"] = kv.second.albums;
        a["tracks"] = kv.second.tracks;
        ar[kv.first] = a;
    }
    j["music_artists"] = ar;
    persist::write_json(index_file(), j);
}

// nftw walk gathering files matching ext set
struct WalkCtx {
    const std::vector<std::string>* exts;
    std::vector<std::string>* out;
};
static thread_local WalkCtx g_walk;

static int walk_cb(const char* fpath, const struct stat* sb, int typeflag, struct FTW* /*ftw*/) {
    if (cancel_scan.load()) return -1;
    if (typeflag != FTW_F) return 0;
    std::string p = fpath;
    std::string ext = utils::get_extension(p);
    for (auto& e : *g_walk.exts) {
        if (ext == e) { g_walk.out->push_back(p); break; }
    }
    return 0;
}

static std::vector<std::string> get_files_recursive(const std::string& root, const std::vector<std::string>& exts) {
    std::vector<std::string> out;
    if (root.empty()) return out;
    g_walk.exts = &exts;
    g_walk.out = &out;
    nftw(root.c_str(), walk_cb, 32, FTW_PHYS);
    return out;
}

static std::string normalize_key(const std::string& v) {
    std::string s = utils::trim(v);
    for (auto& c : s) c = std::tolower((unsigned char)c);
    return s;
}

static std::vector<std::string> split_artists(const std::string& artist_str) {
    std::vector<std::string> out;
    std::unordered_set<std::string> seen;
    for (auto& a : utils::split(artist_str, ',')) {
        std::string t = utils::trim(a);
        if (!t.empty() && !seen.count(t)) {
            seen.insert(t);
            out.push_back(t);
        }
    }
    if (out.empty()) out.push_back("Unknown Artist");
    return out;
}

// Caller must hold data_mutex.
static void add_artist_album(ArtistEntry& art, const std::string& album) {
    for (auto& a : art.albums) if (a == album) return;
    art.albums.push_back(album);
}

static int compare_track_paths(const std::string& a, const std::string& b) {
    auto ita = data.music_files.find(a);
    auto itb = data.music_files.find(b);
    int da = (ita != data.music_files.end() && ita->second.disc_number > 0) ? ita->second.disc_number : 1;
    int db = (itb != data.music_files.end() && itb->second.disc_number > 0) ? itb->second.disc_number : 1;
    if (da != db) return da - db;
    int ta = (ita != data.music_files.end() && ita->second.track_number > 0) ? ita->second.track_number : INT32_MAX;
    int tb = (itb != data.music_files.end() && itb->second.track_number > 0) ? itb->second.track_number : INT32_MAX;
    if (ta != tb) return ta - tb;
    std::string sa = (ita != data.music_files.end() && !ita->second.title.empty()) ? ita->second.title : utils::get_track_name(a);
    std::string sb = (itb != data.music_files.end() && !itb->second.title.empty()) ? itb->second.title : utils::get_track_name(b);
    for (auto& c : sa) c = std::tolower((unsigned char)c);
    for (auto& c : sb) c = std::tolower((unsigned char)c);
    if (sa != sb) return sa < sb ? -1 : 1;
    return a < b ? -1 : (a > b ? 1 : 0);
}

static void sort_music_collections() {
    for (auto& kv : data.music_albums) {
        std::sort(kv.second.tracks.begin(), kv.second.tracks.end(),
            [](const std::string& a, const std::string& b) { return compare_track_paths(a, b) < 0; });
    }
    for (auto& kv : data.music_artists) {
        std::sort(kv.second.tracks.begin(), kv.second.tracks.end(),
            [](const std::string& a, const std::string& b) { return compare_track_paths(a, b) < 0; });
        std::sort(kv.second.albums.begin(), kv.second.albums.end(),
            [](const std::string& a, const std::string& b) {
                std::string la = a, lb = b;
                for (auto& c : la) c = std::tolower((unsigned char)c);
                for (auto& c : lb) c = std::tolower((unsigned char)c);
                return la < lb;
            });
    }
}

// Caller must hold data_mutex.
static bool add_music_file_locked(const std::string& path) {
    if (data.music_files.count(path)) return false;
    metadata::Tags t = metadata::get_tags(path);
    MusicFileInfo m;
    m.title        = !t.title.empty() ? t.title : utils::get_track_name(path);
    m.artist       = !t.artist.empty() ? t.artist : "Unknown Artist";
    m.album        = !t.album.empty() ? t.album : "Unknown Album";
    m.album_artist = utils::trim(t.album_artist);
    m.track_number = t.track_number;
    m.disc_number  = t.disc_number;
    data.music_files[path] = m;

    auto artists = split_artists(m.artist);
    std::string primary_artist = artists[0];
    for (auto& a : artists) {
        auto& ent = data.music_artists[a];
        if (ent.name.empty()) ent.name = a;
        ent.tracks.push_back(path);
        add_artist_album(ent, m.album);
    }

    std::string album_group_key = !m.album_artist.empty() ? m.album_artist : utils::get_dirname(path);
    if (album_group_key.empty()) album_group_key = primary_artist;
    std::string album_display_artist = !m.album_artist.empty() ? m.album_artist : primary_artist;
    std::string album_key = normalize_key(m.album) + "::" + normalize_key(album_group_key);

    auto& ae = data.music_albums[album_key];
    if (ae.name.empty()) {
        ae.name = m.album;
        ae.artist = album_display_artist;
    } else if (!m.album_artist.empty()) {
        ae.artist = m.album_artist;
    }
    ae.tracks.push_back(path);
    return true;
}

static std::string make_safe_filename(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) out.push_back(c);
        else out.push_back('_');
    }
    return out;
}

// Box-filter downscale source RGBA -> dst RGBA. tw,th must be < sw,sh.
static void box_downscale_rgba(const unsigned char* src, int sw, int sh,
                               unsigned char* dst, int tw, int th) {
    for (int y = 0; y < th; ++y) {
        int sy0 = (y * sh) / th;
        int sy1 = std::max(sy0 + 1, ((y + 1) * sh) / th);
        for (int x = 0; x < tw; ++x) {
            int sx0 = (x * sw) / tw;
            int sx1 = std::max(sx0 + 1, ((x + 1) * sw) / tw);
            unsigned int r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int yy = sy0; yy < sy1; ++yy) {
                const unsigned char* row = src + (yy * sw + sx0) * 4;
                for (int xx = sx0; xx < sx1; ++xx) {
                    r += row[0]; g += row[1]; b += row[2]; a += row[3];
                    row += 4;
                    ++n;
                }
            }
            unsigned char* d = dst + (y * tw + x) * 4;
            d[0] = (unsigned char)(r / n);
            d[1] = (unsigned char)(g / n);
            d[2] = (unsigned char)(b / n);
            d[3] = (unsigned char)(a / n);
        }
    }
}

// Resize source image to thumb_size on longest edge, save as PNG. Returns thumb path or "".
static std::string resize_to_thumb(const std::string& src_path, const std::string& safe_name) {
    int sw = 0, sh = 0, ch = 0;
    unsigned char* pixels = stbi_load(src_path.c_str(), &sw, &sh, &ch, 4);
    if (!pixels) return "";
    const int thumb_size = 120;
    float scale = (float)thumb_size / (float)std::max(sw, sh);
    std::string thumb_path = utils::thumb_dir() + "/" + safe_name + ".png";
    if (scale >= 1.0f) {
        // No resize needed; write source as PNG.
        stbi_write_png(thumb_path.c_str(), sw, sh, 4, pixels, sw * 4);
        stbi_image_free(pixels);
        return thumb_path;
    }
    int tw = std::max(1, (int)(sw * scale));
    int th = std::max(1, (int)(sh * scale));
    std::vector<unsigned char> dst((size_t)tw * th * 4);
    box_downscale_rgba(pixels, sw, sh, dst.data(), tw, th);
    stbi_image_free(pixels);
    if (!stbi_write_png(thumb_path.c_str(), tw, th, 4, dst.data(), tw * 4)) return "";
    return thumb_path;
}

// Generate thumb from already-decoded RGBA byte array (e.g. embedded cover).
static std::string resize_bytes_to_thumb(const std::vector<unsigned char>& bytes,
                                         const std::string& safe_name) {
    int sw = 0, sh = 0, ch = 0;
    unsigned char* pixels = stbi_load_from_memory(bytes.data(), (int)bytes.size(),
                                                   &sw, &sh, &ch, 4);
    if (!pixels) return "";
    const int thumb_size = 120;
    float scale = (float)thumb_size / (float)std::max(sw, sh);
    std::string thumb_path = utils::thumb_dir() + "/" + safe_name + ".png";
    if (scale >= 1.0f) {
        stbi_write_png(thumb_path.c_str(), sw, sh, 4, pixels, sw * 4);
        stbi_image_free(pixels);
        return thumb_path;
    }
    int tw = std::max(1, (int)(sw * scale));
    int th = std::max(1, (int)(sh * scale));
    std::vector<unsigned char> dst((size_t)tw * th * 4);
    box_downscale_rgba(pixels, sw, sh, dst.data(), tw, th);
    stbi_image_free(pixels);
    if (!stbi_write_png(thumb_path.c_str(), tw, th, 4, dst.data(), tw * 4)) return "";
    return thumb_path;
}

static void generate_album_thumbnails_locked() {
    for (auto& kv : data.music_albums) {
        if (kv.second.tracks.empty()) continue;
        if (!kv.second.thumb_path.empty()) continue;
        const std::string& cover_track = kv.second.tracks[0];
        std::vector<unsigned char> img_bytes;
        std::string ext;
        if (!metadata::find_folder_cover(cover_track, img_bytes, ext)) continue;
        if (img_bytes.empty()) continue;
        std::string safe = "album_" + make_safe_filename(kv.first);
        std::string thumb_path = resize_bytes_to_thumb(img_bytes, safe);
        if (!thumb_path.empty()) kv.second.thumb_path = thumb_path;
    }
}

static void do_scan_full(const std::string& photo_dir, const std::string& music_dir, const std::string& video_dir) {
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        // keep photos thumbnail data for re-use
        auto old_photos = data.photos;
        data = Data{};
        data.photos = old_photos;
    }
    set_progress("Scanning Videos...");
    auto videos = get_files_recursive(video_dir, EXT_VIDEO);
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        data.videos = std::move(videos);
    }
    set_progress("Scanning Music...");
    auto music_files = get_files_recursive(music_dir, EXT_MUSIC);
    // Expand multi-track chiptune files into per-subtune entries.
    std::vector<std::string> expanded;
    expanded.reserve(music_files.size());
    for (const auto& p : music_files) {
        if (chiptune::is_chiptune(p)) {
            int n = chiptune::subtune_count(p);
            if (n > 1) {
                for (int i = 0; i < n; ++i) expanded.push_back(p + "#" + std::to_string(i));
                continue;
            }
        }
        expanded.push_back(p);
    }
    music_files = std::move(expanded);
    int total = (int)music_files.size();
    for (int i = 0; i < total; ++i) {
        if ((i % 5) == 0) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Indexing Music (%d/%d)", i + 1, total);
            set_progress(buf);
        }
        std::lock_guard<std::mutex> lock(data_mutex);
        add_music_file_locked(music_files[i]);
    }
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        sort_music_collections();
        set_progress("Generating Album Thumbnails...");
        generate_album_thumbnails_locked();
    }
    set_progress("Scanning Photos...");
    auto photo_files = get_files_recursive(photo_dir, EXT_PHOTO);
    int photo_total = (int)photo_files.size();
    for (int i = 0; i < photo_total; ++i) {
        if ((i % 10) == 0) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Indexing Photos (%d/%d)", i + 1, photo_total);
            set_progress(buf);
        }
        const std::string& p = photo_files[i];
        std::string thumb_path;
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            auto it = data.photos.find(p);
            if (it != data.photos.end() && !it->second.thumb_path.empty() &&
                persist::file_exists(it->second.thumb_path)) {
                thumb_path = it->second.thumb_path;
            }
        }
        if (thumb_path.empty()) {
            std::string safe = "photo_" + make_safe_filename(p);
            thumb_path = resize_to_thumb(p, safe);
        }
        std::lock_guard<std::mutex> lock(data_mutex);
        PhotoInfo info;
        info.thumb_path = thumb_path;
        data.photos[p] = info;
    }
    {
        // Prune entries that are no longer present.
        std::lock_guard<std::mutex> lock(data_mutex);
        std::unordered_set<std::string> kept_set(photo_files.begin(), photo_files.end());
        for (auto it = data.photos.begin(); it != data.photos.end(); ) {
            if (!kept_set.count(it->first)) it = data.photos.erase(it);
            else ++it;
        }
    }
    save();
    set_progress("Done");
}

void scan_async(const std::string& photo_dir, const std::string& music_dir, const std::string& video_dir) {
    if (is_scanning.load()) return;
    is_scanning = true;
    if (g_thread.joinable()) g_thread.join();
    g_thread = std::thread([=]() {
        do_scan_full(photo_dir, music_dir, video_dir);
        is_scanning = false;
    });
}

void scan_for_new_async(const std::string& photo_dir, const std::string& music_dir, const std::string& video_dir) {
    // Stub: same as full scan for now. Pruning of stale entries TODO.
    scan_async(photo_dir, music_dir, video_dir);
}

void wait_finish() {
    if (g_thread.joinable()) g_thread.join();
}

} // namespace indexing
