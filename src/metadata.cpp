// Uses TagLib for tag + cover extraction. Folder cover heuristic mirrors metadata.lua.
// Falls back to chiptune (libxmp/libgme) tag readers for module/chip formats.
#include "metadata.h"
#include "chiptune.h"
#include "persist.h"
#include "utils.h"
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/tag.h>
#include <taglib/taglib.h>
#include <taglib/tpropertymap.h>
#include <taglib/xiphcomment.h>

namespace metadata {

static int parse_track_index(const std::string& s) {
    int v = 0; bool any = false;
    for (char c : s) {
        if (std::isdigit((unsigned char)c)) { v = v * 10 + (c - '0'); any = true; }
        else if (any) break;
    }
    return any ? v : -1;
}

static std::string detect_image_ext_from_data(const std::vector<unsigned char>& d) {
    if (d.size() >= 4 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') return "png";
    if (d.size() >= 2 && d[0] == 0xFF && d[1] == 0xD8) return "jpg";
    return "";
}

// Per-dir cache: avoids re-reading 16+ candidate cover files for every track in same album.
struct CoverCacheEntry {
    bool present = false;
    std::vector<unsigned char> data;
    std::string ext;
};
static std::unordered_map<std::string, CoverCacheEntry> g_cover_cache;
static std::mutex g_cover_cache_mutex;

bool find_folder_cover(const std::string& track_path, std::vector<unsigned char>& out, std::string& out_ext) {
    std::string dir = utils::get_dirname(track_path);
    if (dir.empty()) return false;
    {
        std::lock_guard<std::mutex> lock(g_cover_cache_mutex);
        auto it = g_cover_cache.find(dir);
        if (it != g_cover_cache.end()) {
            if (!it->second.present) return false;
            out     = it->second.data;
            out_ext = it->second.ext;
            return true;
        }
    }
    std::string folder_name = utils::get_filename(dir);
    std::vector<std::string> exts = {"jpg", "jpeg", "png", "bmp"};
    std::vector<std::string> candidates;
    for (auto& e : exts) candidates.push_back(folder_name + "." + e);
    static const char* standard[] = {
        "cover.jpg","cover.png","folder.jpg","folder.png",
        "Cover.jpg","Cover.png","Folder.jpg","Folder.png",
        "front.jpg","front.png","Front.jpg","Front.png",
        "album.jpg","album.png","Album.jpg","Album.png", nullptr
    };
    for (int i = 0; standard[i]; ++i) candidates.push_back(standard[i]);
    auto store_cache = [&](bool present) {
        std::lock_guard<std::mutex> lock(g_cover_cache_mutex);
        CoverCacheEntry e;
        e.present = present;
        if (present) { e.data = out; e.ext = out_ext; }
        g_cover_cache[dir] = std::move(e);
    };

    for (auto& name : candidates) {
        std::string full = dir + "/" + name;
        std::string s;
        if (persist::read_file(full, s) && !s.empty()) {
            out.assign(s.begin(), s.end());
            size_t dot = name.find_last_of('.');
            out_ext = (dot == std::string::npos) ? "" : name.substr(dot + 1);
            std::transform(out_ext.begin(), out_ext.end(), out_ext.begin(), ::tolower);
            store_cache(true);
            return true;
        }
    }
    // Fallback: scan directory for any image
    DIR* d = opendir(dir.c_str());
    if (d) {
        while (auto* e = readdir(d)) {
            if (e->d_name[0] == '.') continue;
            std::string n = e->d_name;
            std::string ext = utils::get_extension(n);
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
                std::string full = dir + "/" + n;
                std::string s;
                if (persist::read_file(full, s) && !s.empty()) {
                    out.assign(s.begin(), s.end());
                    out_ext = ext.substr(1);
                    closedir(d);
                    store_cache(true);
                    return true;
                }
            }
        }
        closedir(d);
    }
    store_cache(false);
    return false;
}

Tags get_tags(const std::string& filepath) {
    Tags t;
    // Chiptune/module path: TagLib doesn't know these. Use libxmp/libgme directly.
    if (chiptune::is_chiptune(filepath)) {
        auto ct = chiptune::read_tags(filepath);
        t.title  = ct.title;
        t.artist = ct.artist;
        t.album  = ct.album;
        // Cover art from folder heuristic still applies.
        find_folder_cover(filepath, t.cover_data, t.cover_ext);
        return t;
    }
    TagLib::FileRef ref(filepath.c_str(), true);
    if (!ref.isNull() && ref.tag()) {
        auto* tag = ref.tag();
        t.title  = tag->title().to8Bit(true);
        t.artist = tag->artist().to8Bit(true);
        t.album  = tag->album().to8Bit(true);
        TagLib::PropertyMap pm = ref.file()->properties();
        if (pm.contains("ALBUMARTIST")) {
            t.album_artist = pm["ALBUMARTIST"].toString().to8Bit(true);
        }
        if (pm.contains("TRACKNUMBER")) {
            t.track_number = parse_track_index(pm["TRACKNUMBER"].toString().to8Bit(true));
        }
        if (pm.contains("DISCNUMBER")) {
            t.disc_number = parse_track_index(pm["DISCNUMBER"].toString().to8Bit(true));
        }
    }

    // Cover art: try MPEG ID3v2 APIC, then FLAC pictures.
    {
        TagLib::MPEG::File mp(filepath.c_str(), true);
        if (mp.isValid() && mp.ID3v2Tag()) {
            auto frames = mp.ID3v2Tag()->frameList("APIC");
            if (!frames.isEmpty()) {
                auto* pic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
                if (pic) {
                    auto bv = pic->picture();
                    t.cover_data.assign(bv.data(), bv.data() + bv.size());
                    std::string mime = pic->mimeType().to8Bit();
                    std::transform(mime.begin(), mime.end(), mime.begin(), ::tolower);
                    if (mime.find("png") != std::string::npos) t.cover_ext = "png";
                    else t.cover_ext = "jpg";
                    if (t.cover_ext.empty()) t.cover_ext = detect_image_ext_from_data(t.cover_data);
                }
            }
        }
    }
    if (t.cover_data.empty()) {
        TagLib::FLAC::File ff(filepath.c_str(), true);
        if (ff.isValid()) {
            const auto& pics = ff.pictureList();
            if (!pics.isEmpty()) {
                auto* pic = pics.front();
                auto bv = pic->data();
                t.cover_data.assign(bv.data(), bv.data() + bv.size());
                std::string mime = pic->mimeType().to8Bit();
                std::transform(mime.begin(), mime.end(), mime.begin(), ::tolower);
                if (mime.find("png") != std::string::npos) t.cover_ext = "png";
                else t.cover_ext = "jpg";
            }
        }
    }
    if (t.cover_data.empty()) {
        // Ogg Opus: cover art lives in XiphComment as base64 METADATA_BLOCK_PICTURE.
        std::string ext_lower = utils::get_extension(filepath);
        std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
        if (ext_lower == ".opus") {
            TagLib::Ogg::Opus::File of(filepath.c_str(), true);
            if (of.isValid() && of.tag()) {
                const auto& pics = of.tag()->pictureList();
                if (!pics.isEmpty()) {
                    auto* pic = pics.front();
                    auto bv = pic->data();
                    t.cover_data.assign(bv.data(), bv.data() + bv.size());
                    std::string mime = pic->mimeType().to8Bit();
                    std::transform(mime.begin(), mime.end(), mime.begin(), ::tolower);
                    if (mime.find("png") != std::string::npos) t.cover_ext = "png";
                    else t.cover_ext = "jpg";
                }
            }
        }
    }
    if (t.cover_data.empty()) {
        find_folder_cover(filepath, t.cover_data, t.cover_ext);
    }
    return t;
}

} // namespace metadata
