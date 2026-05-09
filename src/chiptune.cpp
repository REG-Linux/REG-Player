// libxmp (trackers) + libgme (chiptune) decoder + tag reader.
// Decodes whole file to mono float PCM for unified playback w/ ma_audio_buffer.
#include "chiptune.h"
#include "utils.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <gme/gme.h>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <xmp.h>
#include <zlib.h>
#include "StSoundLibrary.h"

namespace chiptune {

static const std::vector<std::string>& xmp_exts() {
    static const std::vector<std::string> e = {
        ".mod", ".xm", ".it", ".s3m", ".mtm", ".stm", ".669", ".far", ".ult",
        ".ams", ".med", ".okt", ".dbm", ".liq", ".mdl", ".mt2", ".pt36",
        ".ptm", ".rtm", ".gdm", ".imf", ".dsm", ".psm", ".dt"
    };
    return e;
}

static const std::vector<std::string>& gme_exts() {
    static const std::vector<std::string> e = {
        ".nsf", ".nsfe", ".gbs", ".spc", ".vgm", ".vgz",
        ".gym", ".ay", ".hes", ".sap", ".kss"
    };
    return e;
}

static const std::vector<std::string>& stsound_exts() {
    static const std::vector<std::string> e = {".ym", ".sndh"};
    return e;
}

static const std::vector<std::string>& csid_exts() {
    static const std::vector<std::string> e = {".sid"};
    return e;
}

static bool is_stsound_ext(const std::string& ext) {
    for (auto& e : stsound_exts()) if (ext == e) return true;
    return false;
}
static bool is_csid_ext(const std::string& ext) {
    for (auto& e : csid_exts()) if (ext == e) return true;
    return false;
}

extern "C" {
int csid_render(const unsigned char* bytes, int size, int subtune_idx,
                int sample_rate, short* out_buf, int max_int16_samples);
int csid_subtune_count(const unsigned char* bytes, int size);
void csid_get_tags(const unsigned char* bytes, int size,
                   char* title, char* author, char* info);
}

const std::vector<std::string>& extensions() {
    static std::vector<std::string> all;
    if (all.empty()) {
        for (auto& e : xmp_exts())     all.push_back(e);
        for (auto& e : gme_exts())     all.push_back(e);
        for (auto& e : stsound_exts()) all.push_back(e);
        for (auto& e : csid_exts())    all.push_back(e);
    }
    return all;
}

static bool is_xmp_ext(const std::string& ext) {
    for (auto& e : xmp_exts()) if (ext == e) return true;
    return false;
}
static bool is_gme_ext(const std::string& ext) {
    for (auto& e : gme_exts()) if (ext == e) return true;
    return false;
}

void split_subtune(const std::string& full, std::string& base, int& track) {
    track = 0;
    base = full;
    size_t hash = full.find_last_of('#');
    if (hash == std::string::npos) return;
    std::string suffix = full.substr(hash + 1);
    if (suffix.empty()) return;
    int v = 0;
    bool any = false;
    for (char c : suffix) {
        if (c < '0' || c > '9') return;
        v = v * 10 + (c - '0');
        any = true;
    }
    if (!any) return;
    track = v;
    base = full.substr(0, hash);
}

bool is_chiptune(const std::string& path) {
    std::string base; int t;
    split_subtune(path, base, t);
    std::string ext = utils::get_extension(base);
    return is_xmp_ext(ext) || is_gme_ext(ext) || is_stsound_ext(ext) || is_csid_ext(ext);
}

int subtune_count(const std::string& path) {
    std::string base; int t;
    split_subtune(path, base, t);
    std::string ext = utils::get_extension(base);
    if (is_xmp_ext(ext)) return 1;
    if (is_stsound_ext(ext)) return 1;
    if (is_csid_ext(ext)) {
        std::vector<uint8_t> data;
        std::ifstream f(base, std::ios::binary);
        if (!f) return 1;
        std::ostringstream ss; ss << f.rdbuf();
        std::string raw = ss.str();
        if (raw.empty()) return 1;
        int n = csid_subtune_count((const unsigned char*)raw.data(), (int)raw.size());
        if (n > 99) n = 99;
        return n;
    }
    if (!is_gme_ext(ext)) return 1;
    Music_Emu* emu = nullptr;
    if (gme_open_file(base.c_str(), &emu, 44100) || !emu) return 1;
    int n = gme_track_count(emu);
    gme_delete(emu);
    if (n <= 0) return 1;
    // Some formats (HES, KSS) report 256 as default when count is unspecified.
    // Cap to avoid spurious entries.
    if (n > 99) n = 99;
    return n;
}

// Append int16 stereo to float stereo buffer + float mono mixdown.
static void s16_stereo_to_floats(const int16_t* src, size_t frames,
                                 std::vector<float>& stereo,
                                 std::vector<float>& mono) {
    size_t off_s = stereo.size();
    size_t off_m = mono.size();
    stereo.resize(off_s + frames * 2);
    mono.resize(off_m + frames);
    const float inv = 1.0f / 32768.0f;
    for (size_t i = 0; i < frames; ++i) {
        float l = src[i * 2 + 0] * inv;
        float r = src[i * 2 + 1] * inv;
        stereo[off_s + i * 2 + 0] = l;
        stereo[off_s + i * 2 + 1] = r;
        mono[off_m + i] = (l + r) * 0.5f;
    }
}

static int decode_xmp(const std::string& path, std::vector<float>& stereo,
                      std::vector<float>& mono, int max_seconds) {
    xmp_context ctx = xmp_create_context();
    if (!ctx) return 0;
    if (xmp_load_module(ctx, const_cast<char*>(path.c_str())) < 0) {
        xmp_free_context(ctx);
        return 0;
    }
    const int sr = 44100;
    if (xmp_start_player(ctx, sr, 0) < 0) {
        xmp_release_module(ctx);
        xmp_free_context(ctx);
        return 0;
    }
    xmp_set_player(ctx, XMP_PLAYER_INTERP, XMP_INTERP_LINEAR);

    const int chunk_frames = 4096;
    const int chunk_bytes  = chunk_frames * 2 * 2;
    std::vector<int16_t> chunk(chunk_frames * 2);

    stereo.reserve((size_t)sr * 2 * std::min(max_seconds, 60));
    mono.reserve((size_t)sr * std::min(max_seconds, 60));
    int total_frames = 0;
    int max_frames = sr * max_seconds;

    while (total_frames < max_frames) {
        int rc = xmp_play_buffer(ctx, chunk.data(), chunk_bytes, 0);
        if (rc < 0) break;
        s16_stereo_to_floats(chunk.data(), chunk_frames, stereo, mono);
        total_frames += chunk_frames;
    }

    xmp_end_player(ctx);
    xmp_release_module(ctx);
    xmp_free_context(ctx);
    return mono.empty() ? 0 : sr;
}

static int decode_gme(const std::string& path, std::vector<float>& stereo,
                      std::vector<float>& mono, int max_seconds) {
    const int sr = 44100;
    std::string base; int track_idx = 0;
    split_subtune(path, base, track_idx);
    Music_Emu* emu = nullptr;
    gme_err_t err = gme_open_file(base.c_str(), &emu, sr);
    if (err || !emu) return 0;
    int total = gme_track_count(emu);
    if (track_idx < 0 || track_idx >= total) track_idx = 0;
    err = gme_start_track(emu, track_idx);
    if (err) { gme_delete(emu); return 0; }

    int track_len = max_seconds * 1000;
    gme_info_t* info = nullptr;
    if (gme_track_info(emu, &info, track_idx) == nullptr && info) {
        if (info->play_length > 0 && info->play_length < track_len) track_len = info->play_length;
        else if (info->length    > 0 && info->length    < track_len) track_len = info->length;
        gme_free_info(info);
    }
    gme_set_fade(emu, std::max(0, track_len - 4000));

    const int chunk_frames = 4096;
    std::vector<int16_t> chunk(chunk_frames * 2);

    stereo.reserve((size_t)sr * 2 * std::min(max_seconds, 60));
    mono.reserve((size_t)sr * std::min(max_seconds, 60));
    int total_frames = 0;
    int max_frames = sr * max_seconds;

    while (total_frames < max_frames) {
        if (gme_track_ended(emu)) break;
        err = gme_play(emu, chunk_frames * 2, chunk.data());
        if (err) break;
        s16_stereo_to_floats(chunk.data(), chunk_frames, stereo, mono);
        total_frames += chunk_frames;
    }
    gme_delete(emu);
    return mono.empty() ? 0 : sr;
}

// StSound: mono int16 output. Duplicate to stereo.
static int decode_stsound(const std::string& path, std::vector<float>& stereo,
                          std::vector<float>& mono, int max_seconds) {
    const int sr = 44100;
    YMMUSIC* ym = ymMusicCreate();
    if (!ym) return 0;
    if (!ymMusicLoad(ym, path.c_str())) { ymMusicDestroy(ym); return 0; }
    ymMusicSetLoopMode(ym, YMFALSE);
    ymMusicPlay(ym);

    const int chunk_frames = 4096;
    std::vector<int16_t> chunk(chunk_frames);
    int total_frames = 0;
    int max_frames = sr * max_seconds;
    stereo.reserve((size_t)sr * 2 * std::min(max_seconds, 60));
    mono.reserve((size_t)sr * std::min(max_seconds, 60));

    while (total_frames < max_frames) {
        if (!ymMusicCompute(ym, chunk.data(), chunk_frames)) break;
        size_t off_s = stereo.size();
        size_t off_m = mono.size();
        stereo.resize(off_s + chunk_frames * 2);
        mono.resize(off_m + chunk_frames);
        const float inv = 1.0f / 32768.0f;
        for (int i = 0; i < chunk_frames; ++i) {
            float v = chunk[i] * inv;
            stereo[off_s + i * 2 + 0] = v;
            stereo[off_s + i * 2 + 1] = v;
            mono[off_m + i] = v;
        }
        total_frames += chunk_frames;
    }
    ymMusicStop(ym);
    ymMusicDestroy(ym);
    return mono.empty() ? 0 : sr;
}

// SID via cSID-light: mono int16 → duplicated stereo + mono mixdown.
static int decode_csid(const std::string& path, int subtune_idx,
                       std::vector<float>& stereo, std::vector<float>& mono,
                       int max_seconds) {
    const int sr = 44100;
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    std::ostringstream ss; ss << f.rdbuf();
    std::string raw = ss.str();
    if (raw.empty()) return 0;

    int frames = sr * max_seconds;
    std::vector<int16_t> tmp(frames);
    int n = csid_render((const unsigned char*)raw.data(), (int)raw.size(),
                        subtune_idx, sr, tmp.data(), frames);
    if (n <= 0) return 0;
    const float inv = 1.0f / 32768.0f;
    stereo.reserve((size_t)n * 2);
    mono.reserve((size_t)n);
    for (int i = 0; i < n; ++i) {
        float v = tmp[i] * inv;
        stereo.push_back(v);
        stereo.push_back(v);
        mono.push_back(v);
    }
    return sr;
}

int decode_to_pcm(const std::string& path,
                  std::vector<float>& stereo,
                  std::vector<float>& mono,
                  int max_seconds) {
    stereo.clear();
    mono.clear();
    std::string base; int sub_idx = 0;
    split_subtune(path, base, sub_idx);
    std::string ext = utils::get_extension(base);
    if (is_xmp_ext(ext))     return decode_xmp(base, stereo, mono, max_seconds);
    if (is_gme_ext(ext))     return decode_gme(path, stereo, mono, max_seconds);
    if (is_stsound_ext(ext)) return decode_stsound(base, stereo, mono, max_seconds);
    if (is_csid_ext(ext))    return decode_csid(base, sub_idx, stereo, mono, max_seconds);
    return 0;
}

// Read entire file to memory. For .vgz, gunzip transparently.
static bool slurp_vgm_data(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string raw = ss.str();
    if (raw.empty()) return false;
    if (raw.size() >= 2 && (uint8_t)raw[0] == 0x1f && (uint8_t)raw[1] == 0x8b) {
        // gzip — decompress with zlib (windowBits 15 + 16 for gzip wrapper)
        z_stream zs{};
        zs.next_in  = (Bytef*)raw.data();
        zs.avail_in = (uInt)raw.size();
        if (inflateInit2(&zs, 15 + 16) != Z_OK) return false;
        std::vector<uint8_t> buf;
        buf.reserve(raw.size() * 4);
        uint8_t chunk[8192];
        while (true) {
            zs.next_out  = chunk;
            zs.avail_out = sizeof(chunk);
            int rc = inflate(&zs, Z_NO_FLUSH);
            buf.insert(buf.end(), chunk, chunk + (sizeof(chunk) - zs.avail_out));
            if (rc == Z_STREAM_END) break;
            if (rc != Z_OK) { inflateEnd(&zs); return false; }
        }
        inflateEnd(&zs);
        out = std::move(buf);
    } else {
        out.assign(raw.begin(), raw.end());
    }
    return true;
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Convert UTF-16-LE (null-terminated) to UTF-8. Advances cursor past null.
static std::string utf16le_to_utf8(const uint8_t* data, size_t size, size_t& cursor) {
    std::string out;
    while (cursor + 1 < size) {
        uint16_t c = (uint16_t)data[cursor] | ((uint16_t)data[cursor + 1] << 8);
        cursor += 2;
        if (c == 0) break;
        if (c < 0x80) {
            out.push_back((char)c);
        } else if (c < 0x800) {
            out.push_back((char)(0xC0 | (c >> 6)));
            out.push_back((char)(0x80 | (c & 0x3F)));
        } else {
            out.push_back((char)(0xE0 | (c >> 12)));
            out.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

// Parse GD3 block. Fills t with available fields.
// GD3 layout: "Gd3 " magic, u32 version, u32 size, then 11 UTF-16-LE \0-terminated strings:
// track-en, track-jp, game-en, game-jp, system-en, system-jp,
// author-en, author-jp, release-date, dumper, notes
static bool parse_gd3(const std::vector<uint8_t>& data, Tags& t) {
    if (data.size() < 0x40) return false;
    // VGM header: at offset 0x14, u32 = relative offset to GD3 from 0x14, or 0 if no GD3.
    uint32_t gd3_off_rel = le32(data.data() + 0x14);
    if (gd3_off_rel == 0) return false;
    uint32_t gd3_off = 0x14 + gd3_off_rel;
    if (gd3_off + 12 > data.size()) return false;
    if (std::memcmp(data.data() + gd3_off, "Gd3 ", 4) != 0) return false;
    uint32_t gd3_size = le32(data.data() + gd3_off + 8);
    size_t base = gd3_off + 12;
    size_t end  = base + gd3_size;
    if (end > data.size()) end = data.size();

    size_t cursor = base;
    std::string track_en  = utf16le_to_utf8(data.data(), end, cursor);
    std::string track_jp  = utf16le_to_utf8(data.data(), end, cursor);
    std::string game_en   = utf16le_to_utf8(data.data(), end, cursor);
    std::string game_jp   = utf16le_to_utf8(data.data(), end, cursor);
    std::string system_en = utf16le_to_utf8(data.data(), end, cursor);
    std::string system_jp = utf16le_to_utf8(data.data(), end, cursor);
    std::string author_en = utf16le_to_utf8(data.data(), end, cursor);
    std::string author_jp = utf16le_to_utf8(data.data(), end, cursor);
    std::string date      = utf16le_to_utf8(data.data(), end, cursor);
    std::string dumper    = utf16le_to_utf8(data.data(), end, cursor);
    std::string notes     = utf16le_to_utf8(data.data(), end, cursor);

    if (!track_en.empty())  t.title  = track_en;
    if (!author_en.empty()) t.artist = author_en;
    if (!game_en.empty())   t.album  = game_en;
    if (!system_en.empty()) t.system = system_en;
    if (!game_jp.empty())   t.game_jp = game_jp;
    if (!date.empty())      t.release_date = date;
    if (!dumper.empty())    t.dumper = dumper;
    if (!notes.empty())     t.comment = notes;
    return !track_en.empty() || !author_en.empty() || !game_en.empty() || !system_en.empty();
}

Tags read_tags(const std::string& path) {
    Tags t;
    std::string base; int track_idx = 0;
    split_subtune(path, base, track_idx);
    std::string ext = utils::get_extension(base);
    if (is_xmp_ext(ext)) {
        xmp_context ctx = xmp_create_context();
        if (ctx) {
            if (xmp_load_module(ctx, const_cast<char*>(base.c_str())) == 0) {
                xmp_module_info mi;
                xmp_get_module_info(ctx, &mi);
                if (mi.mod) {
                    if (mi.mod->name[0])  t.title  = mi.mod->name;
                    if (mi.mod->type[0])  t.album  = std::string("Tracker — ") + mi.mod->type;
                }
                xmp_release_module(ctx);
                t.has_value = !t.title.empty() || !t.album.empty();
            }
            xmp_free_context(ctx);
        }
    } else if (is_csid_ext(ext)) {
        std::ifstream f(base, std::ios::binary);
        if (f) {
            std::ostringstream ss; ss << f.rdbuf();
            std::string raw = ss.str();
            if (!raw.empty()) {
                char title[32] = {0}, author[32] = {0}, info[32] = {0};
                csid_get_tags((const unsigned char*)raw.data(), (int)raw.size(),
                              title, author, info);
                if (title[0])  t.title    = title;
                if (author[0]) t.artist   = author;
                if (info[0])   t.copyright = info;
                t.album = "Commodore 64 SID";
                t.has_value = !t.title.empty() || !t.artist.empty();
            }
        }
        if (t.title.empty()) t.title = utils::get_track_name(base);
        return t;
    } else if (is_stsound_ext(ext)) {
        YMMUSIC* ym = ymMusicCreate();
        if (ym) {
            if (ymMusicLoad(ym, base.c_str())) {
                ymMusicInfo_t info;
                ymMusicGetInfo(ym, &info);
                if (info.pSongName    && info.pSongName[0])    t.title    = info.pSongName;
                if (info.pSongAuthor  && info.pSongAuthor[0])  t.artist   = info.pSongAuthor;
                if (info.pSongType    && info.pSongType[0])    t.album    = std::string("Atari ST — ") + info.pSongType;
                if (info.pSongComment && info.pSongComment[0]) t.comment  = info.pSongComment;
                t.has_value = !t.title.empty() || !t.artist.empty();
            }
            ymMusicDestroy(ym);
        }
        if (t.title.empty()) t.title = utils::get_track_name(base);
        return t;
    } else if (ext == ".vgm" || ext == ".vgz") {
        std::vector<uint8_t> data;
        if (slurp_vgm_data(base, data)) {
            if (parse_gd3(data, t)) t.has_value = true;
        }
        if (t.title.empty()) t.title = utils::get_track_name(base);
        return t;
    } else if (is_gme_ext(ext)) {
        Music_Emu* emu = nullptr;
        if (!gme_open_file(base.c_str(), &emu, 44100) && emu) {
            gme_info_t* info = nullptr;
            if (!gme_track_info(emu, &info, track_idx) && info) {
                if (info->song      && info->song[0])      t.title     = info->song;
                if (info->author    && info->author[0])    t.artist    = info->author;
                if (info->game      && info->game[0])      t.album     = info->game;
                else if (info->system && info->system[0])  t.album     = info->system;
                if (info->system    && info->system[0])    t.system    = info->system;
                if (info->copyright && info->copyright[0]) t.copyright = info->copyright;
                if (info->dumper    && info->dumper[0])    t.dumper    = info->dumper;
                if (info->comment   && info->comment[0])   t.comment   = info->comment;
                t.length_ms        = info->length;
                t.intro_length_ms  = info->intro_length;
                t.loop_length_ms   = info->loop_length;
                gme_free_info(info);
            }
            gme_delete(emu);
            t.has_value = !t.title.empty() || !t.artist.empty() || !t.album.empty();
        }
    }
    if (t.title.empty()) t.title = utils::get_track_name(path);
    return t;
}

} // namespace chiptune
