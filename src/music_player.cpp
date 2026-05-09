#include "music_player.h"
#include "assets.h"
#include "chiptune.h"
#include "opus_audio.h"
#include "common.h"
#include "miniaudio.h"
#include "metadata.h"
#include "ui.h"
#include "utils.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cstring>

namespace music_player {

State s;
static ma_engine g_engine;
static bool g_engine_ok = false;
static ma_sound g_sound;
static bool g_sound_loaded = false;
// Stereo float for playback (interleaved L,R,L,R).
static std::vector<float> g_pcm_stereo;
// Mono float for visualizer (per-frame averaged L+R).
static std::vector<float> g_pcm;
static int g_pcm_pos = 0;            // frame index (matches mono buffer)
static int g_pcm_sample_rate = 44100;

// Custom data source over our pre-decoded stereo float PCM.
struct ChipDataSource {
    ma_data_source_base base;
    const float* data = nullptr;
    ma_uint64 frames = 0;
    ma_uint64 cursor = 0;             // frames consumed
    ma_uint32 sample_rate = 44100;
    ma_uint32 channels = 2;
};
static ChipDataSource g_chip_ds;
static bool g_chip_active = false;

static ma_result chip_read(ma_data_source* pDS, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
    auto* d = (ChipDataSource*)pDS;
    if (d->cursor >= d->frames) { if (pFramesRead) *pFramesRead = 0; return MA_AT_END; }
    ma_uint64 avail = d->frames - d->cursor;
    ma_uint64 toRead = (frameCount < avail) ? frameCount : avail;
    // Stereo: 2 floats per frame.
    std::memcpy(pFramesOut, d->data + d->cursor * d->channels,
                (size_t)(toRead * d->channels * sizeof(float)));
    d->cursor += toRead;
    if (pFramesRead) *pFramesRead = toRead;
    return MA_SUCCESS;
}
static ma_result chip_seek(ma_data_source* pDS, ma_uint64 frameIndex) {
    auto* d = (ChipDataSource*)pDS;
    if (frameIndex > d->frames) frameIndex = d->frames;
    d->cursor = frameIndex;
    return MA_SUCCESS;
}
static ma_result chip_get_format(ma_data_source* pDS, ma_format* fmt, ma_uint32* ch, ma_uint32* sr, ma_channel*, size_t) {
    auto* d = (ChipDataSource*)pDS;
    if (fmt) *fmt = ma_format_f32;
    if (ch)  *ch  = d->channels;
    if (sr)  *sr  = d->sample_rate;
    return MA_SUCCESS;
}
static ma_result chip_get_cursor(ma_data_source* pDS, ma_uint64* p) {
    auto* d = (ChipDataSource*)pDS;
    if (p) *p = d->cursor;
    return MA_SUCCESS;
}
static ma_result chip_get_length(ma_data_source* pDS, ma_uint64* p) {
    auto* d = (ChipDataSource*)pDS;
    if (p) *p = d->frames;
    return MA_SUCCESS;
}
static const ma_data_source_vtable g_chip_vtable = {
    chip_read, chip_seek, chip_get_format, chip_get_cursor, chip_get_length,
    NULL, // onSetLooping
    0
};

void init() {
    if (!g_engine_ok) {
        if (ma_engine_init(NULL, &g_engine) == MA_SUCCESS) g_engine_ok = true;
    }
}

void stop() {
    if (g_sound_loaded) { ma_sound_uninit(&g_sound); g_sound_loaded = false; }
    if (g_chip_active) { ma_data_source_uninit(&g_chip_ds.base); g_chip_active = false; }
    s.playing = false; s.paused = false; s.elapsed = 0;
    g_pcm.clear(); g_pcm_stereo.clear(); g_pcm_pos = 0;
}

static void release_cover() {
    if (s.cover_art) { SDL_DestroyTexture((SDL_Texture*)s.cover_art); s.cover_art = nullptr; }
}

static bool load_track(const Track& t) {
    stop();
    release_cover();
    s.current_track = t;
    s.elapsed = 0;
    if (!g_engine_ok) return false;

    // Tags
    metadata::Tags mtags = metadata::get_tags(t.path);
    s.tags.title  = !mtags.title.empty()  ? mtags.title  : utils::get_track_name(t.path);
    s.tags.artist = !mtags.artist.empty() ? mtags.artist : "Unknown Artist";
    s.tags.album  = !mtags.album.empty()  ? mtags.album  : "Unknown Album";

    // Pre-load next track tags
    s.next_tags = {};
    if (s.playlist.size() > 1) {
        int next_idx = (s.current_index + 1) % (int)s.playlist.size();
        metadata::Tags nt = metadata::get_tags(s.playlist[next_idx].path);
        s.next_tags.title  = !nt.title.empty()  ? nt.title  : utils::get_track_name(s.playlist[next_idx].path);
        s.next_tags.artist = nt.artist;
        s.next_tags.album  = nt.album;
    }

    // Reset marquees
    for (auto& kv : s.marquees) {
        kv.second.offset = 0;
        kv.second.timer = 0;
        kv.second.phase = "pause_start";
    }

    if (!mtags.cover_data.empty()) {
        SDL_IOStream* io = SDL_IOFromConstMem(mtags.cover_data.data(), (size_t)mtags.cover_data.size());
        if (io) {
            SDL_Texture* tex = IMG_LoadTexture_IO(g_app.renderer, io, true);
            if (tex) {
                SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
                s.cover_art = tex;
            }
        }
    }

    // Opus path: decode whole .opus file via libopusfile (always 48 kHz stereo).
    // Routes through same g_chip_ds vtable since shape (pre-decoded f32 stereo
    // + mono mix) is identical.
    bool is_opus = opus_audio::is_opus(t.path);
    bool is_chip = !is_opus && chiptune::is_chiptune(t.path);

    if (is_opus || is_chip) {
        int sr = is_opus
            ? opus_audio::decode_to_pcm(t.path, g_pcm_stereo, g_pcm)
            : chiptune::decode_to_pcm(t.path, g_pcm_stereo, g_pcm, 300);
        if (sr <= 0 || g_pcm.empty()) return false;
        g_pcm_sample_rate = sr;
        s.duration = (double)g_pcm.size() / (double)sr;

        ma_data_source_config dsc = ma_data_source_config_init();
        dsc.vtable = &g_chip_vtable;
        if (ma_data_source_init(&dsc, &g_chip_ds.base) != MA_SUCCESS) return false;
        g_chip_ds.data = g_pcm_stereo.data();
        g_chip_ds.frames = g_pcm.size();              // frame count = mono buffer size
        g_chip_ds.cursor = 0;
        g_chip_ds.sample_rate = (ma_uint32)sr;
        g_chip_ds.channels = 2;
        g_chip_active = true;

        if (ma_sound_init_from_data_source(&g_engine, &g_chip_ds.base, 0, NULL, &g_sound) != MA_SUCCESS) {
            ma_data_source_uninit(&g_chip_ds.base);
            g_chip_active = false;
            return false;
        }
        g_sound_loaded = true;
        ma_sound_start(&g_sound);
        s.playing = true; s.paused = false;
        return true;
    }

    // Decode whole file as stereo for playback engine consistency + mono mix for visualizer.
    ma_decoder dec;
    ma_decoder_config dcfg = ma_decoder_config_init(ma_format_f32, 2, 0);
    if (ma_decoder_init_file(t.path.c_str(), &dcfg, &dec) == MA_SUCCESS) {
        ma_uint64 frames;
        if (ma_decoder_get_length_in_pcm_frames(&dec, &frames) == MA_SUCCESS && frames > 0) {
            g_pcm_stereo.assign(frames * 2, 0);
            ma_uint64 read = 0;
            ma_decoder_read_pcm_frames(&dec, g_pcm_stereo.data(), frames, &read);
            // Mixdown for visualizer
            g_pcm.assign(frames, 0);
            for (ma_uint64 i = 0; i < frames; ++i) {
                g_pcm[i] = (g_pcm_stereo[i * 2] + g_pcm_stereo[i * 2 + 1]) * 0.5f;
            }
        }
        g_pcm_sample_rate = (int)dec.outputSampleRate;
        if (g_pcm_sample_rate <= 0) g_pcm_sample_rate = 44100;
        ma_decoder_uninit(&dec);
    } else {
        g_pcm_sample_rate = 44100;
    }

    if (ma_sound_init_from_file(&g_engine, t.path.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_STREAM, NULL, NULL, &g_sound) != MA_SUCCESS) {
        return false;
    }
    g_sound_loaded = true;
    float length_sec = 0;
    ma_sound_get_length_in_seconds(&g_sound, &length_sec);
    s.duration = length_sec;
    ma_sound_start(&g_sound);
    s.playing = true; s.paused = false;
    return true;
}

void play(const std::string& filepath, const std::vector<Track>* custom_playlist) {
    if (custom_playlist) s.playlist = *custom_playlist;
    Track found{utils::get_filename(filepath), filepath};
    s.current_index = 0;
    for (size_t i = 0; i < s.playlist.size(); ++i) {
        if (s.playlist[i].path == filepath) {
            s.current_index = (int)i;
            found = s.playlist[i];
            break;
        }
    }
    s.active = true; s.fade_alpha = 0;
    if (s.auto_sleep_minutes > 0) s.auto_sleep_remaining = s.auto_sleep_minutes * 60.0;
    else s.auto_sleep_remaining = -1;
    load_track(found);
}

void toggle_pause() {
    if (!g_sound_loaded) return;
    if (s.paused) { ma_sound_start(&g_sound); s.paused = false; }
    else          { ma_sound_stop(&g_sound);  s.paused = true; }
}

void next_track() {
    if (s.playlist.empty()) return;
    int n = (int)s.playlist.size();
    int tries = 0;
    do {
        s.current_index = (s.current_index + 1) % n;
        if (load_track(s.playlist[s.current_index])) return;
        ++tries;
    } while (tries < n);
    // All tracks failed — close player to avoid infinite loop.
    close();
}

void prev_track() {
    if (s.playlist.empty()) return;
    if (s.elapsed > 3) { load_track(s.playlist[s.current_index]); return; }
    s.current_index = (s.current_index - 1 + (int)s.playlist.size()) % (int)s.playlist.size();
    load_track(s.playlist[s.current_index]);
}

void close() {
    stop();
    s.active = false;
    s.current_track = {};
    s.playlist.clear();
    s.auto_sleep_remaining = -1;
}

void set_repeat_one(bool e) { s.repeat_one = e; }
void set_auto_sleep_minutes(int m) {
    if (m < 0) m = 0;
    if (m > 30) m = 30;
    s.auto_sleep_minutes = m;
    if (m > 0 && s.active) s.auto_sleep_remaining = m * 60.0;
    else s.auto_sleep_remaining = -1;
}
void set_visualizer_mode(const std::string& m) {
    if (m == "off" || m == "wave" || m == "bars") s.visualizer_mode = m;
}

void update(float dt) {
    if (!s.active) return;
    if (s.fade_alpha < 1) s.fade_alpha = std::min(1.0f, s.fade_alpha + dt * 4);
    if (s.auto_sleep_remaining >= 0) {
        s.auto_sleep_remaining -= dt;
        if (s.auto_sleep_remaining <= 0) { s.auto_sleep_remaining = -1; close(); return; }
    }
    if (g_sound_loaded && s.playing && !s.paused) {
        float cur = 0;
        ma_sound_get_cursor_in_seconds(&g_sound, &cur);
        s.elapsed = cur;
        g_pcm_pos = (int)(cur * g_pcm_sample_rate);
        // End-of-track: use at_end (set by miniaudio after fully draining decoder).
        // Avoid !is_playing check — returns false briefly post-start and triggers spurious skips.
        if (ma_sound_at_end(&g_sound)) {
            if (s.repeat_one && s.current_index >= 0 && (size_t)s.current_index < s.playlist.size())
                load_track(s.playlist[s.current_index]);
            else
                next_track();
        }
    }

    // Update marquees (mirror music_player.lua update tail)
    int info_w = (int)(g_app.screen_w * 0.65f);
    if (s.marquees.count("title"))  s.marquees["title"].max_width  = info_w;
    if (s.marquees.count("artist")) s.marquees["artist"].max_width = info_w;
    if (s.marquees.count("album"))  s.marquees["album"].max_width  = info_w;
    if (s.marquees.count("title")) {
        std::string tn = !s.tags.title.empty() ? s.tags.title : utils::get_track_name(s.current_track.name);
        ui::update_marquee(s.marquees["title"], dt, ui::text_width(assets::fonts.title, tn));
    }
    if (s.marquees.count("artist")) {
        std::string an = !s.tags.artist.empty() ? s.tags.artist : "Unknown Artist";
        ui::update_marquee(s.marquees["artist"], dt, ui::text_width(assets::fonts.artist, an));
    }
    if (s.marquees.count("album")) {
        std::string al = !s.tags.album.empty() ? s.tags.album : "Unknown Album";
        ui::update_marquee(s.marquees["album"], dt, ui::text_width(assets::fonts.album, al));
    }
}

const Tags& current_tags() { return s.tags; }
const Tags& next_tags() { return s.next_tags; }

const float* pcm()    { return g_pcm.empty() ? nullptr : g_pcm.data(); }
int pcm_count()       { return (int)g_pcm.size(); }
int pcm_pos()         { return g_pcm_pos; }

} // namespace music_player
