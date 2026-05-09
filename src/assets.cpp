#include "assets.h"
#include "miniaudio.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include "common.h"

namespace assets {

std::unordered_map<std::string, SDL_Texture*> images;
Fonts fonts;

static ma_engine g_audio_engine;
static bool g_audio_engine_ok = false;
static ma_sound g_keytone;
static bool g_keytone_loaded = false;

std::string assets_dir() {
    const char* env = std::getenv("REG_ASSETS_DIR");
    if (env && *env) return env;
#ifdef REG_ASSETS_DIR
    return REG_ASSETS_DIR;
#else
    // Fallback: cwd/assets
    return "assets";
#endif
}

static SDL_Texture* load_image(const std::string& rel) {
    std::string p = assets_dir() + "/" + rel;
    SDL_Texture* t = IMG_LoadTexture(g_app.renderer, p.c_str());
    if (!t) {
        SDL_Log("Failed to load image %s: %s", p.c_str(), SDL_GetError());
    } else {
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    return t;
}

static TTF_Font* load_font(const std::string& rel, int size) {
    std::string p = assets_dir() + "/" + rel;
    TTF_Font* f = TTF_OpenFont(p.c_str(), size);
    if (!f) {
        SDL_Log("Failed to load font %s: %s", p.c_str(), SDL_GetError());
    }
    return f;
}

static void load_icon(const char* name, const char* file) {
    SDL_Texture* t = load_image(std::string("icons/") + file);
    if (t) images[name] = t;
}

void load() {
    if (!TTF_WasInit() && !TTF_Init()) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
    }

    load_icon("folder", "folder.png");
    load_icon("video", "video.png");
    load_icon("music", "music.png");
    load_icon("photo", "photo.png");
    load_icon("drive", "drive.png");
    load_icon("sdcard", "sdcard.png");
    load_icon("info", "info.png");
    load_icon("option", "option.png");
    load_icon("play", "play.png");
    load_icon("pause", "pause.png");
    load_icon("lock", "lock.png");
    load_icon("repeat_one", "repeat-one.png");
    load_icon("battery", "battery.png");
    load_icon("battery_charge", "batterycharge.png");
    load_icon("albums", "albums.png");
    load_icon("album", "album.png");
    load_icon("track", "track.png");
    load_icon("mic", "mic.png");
    load_icon("artist", "artist.png");
    load_icon("folder_music", "folder-music.png");
    load_icon("file_music", "file-music.png");
    load_icon("folder_image", "folder-image.png");
    load_icon("folder_video", "folder-video.png");
    load_icon("file_video", "file-video.png");
    load_icon("folders", "folders.png");
    load_icon("file", "file.png");
    load_icon("theme", "theme.png");
    load_icon("history", "history.png");
    load_icon("quit", "quit.png");
    load_icon("shuffle", "shuffle.png");
    load_icon("eye", "eye.png");
    load_icon("volume_up", "volume-up.png");
    load_icon("volume_down", "volume-down.png");
    load_icon("volume_mute", "volume-mute.png");
    load_icon("brightness", "bulb.png");
    load_icon("settings", "settings.png");

    // Category icon aliases.
    images["cat_settings"] = images["settings"];
    images["cat_photo"]    = images["photo"];
    images["cat_music"]    = images["music"];
    images["cat_video"]    = images["video"];
    images["cat_folder"]   = images["folders"];

    // Fonts
    const std::string font_path = "font/Orbitron-Bold.ttf";
    fonts.main         = load_font(font_path, 28);
    fonts.small        = load_font(font_path, 24);
    fonts.title        = load_font(font_path, 30);
    fonts.artist       = load_font(font_path, 24);
    fonts.album        = load_font(font_path, 22);
    fonts.time_elapsed = load_font(font_path, 28);
    fonts.time_dur     = load_font(font_path, 24);
    fonts.xs           = load_font(font_path, 18);
    fonts.large        = load_font(font_path, 48);

    // Audio engine for SFX
    if (ma_engine_init(NULL, &g_audio_engine) == MA_SUCCESS) {
        g_audio_engine_ok = true;
        std::string sfx = assets_dir() + "/sfx/keytone.wav";
        if (ma_sound_init_from_file(&g_audio_engine, sfx.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, &g_keytone) == MA_SUCCESS) {
            g_keytone_loaded = true;
        }
    }
}

void unload() {
    for (auto& kv : images) {
        if (kv.second) SDL_DestroyTexture(kv.second);
    }
    images.clear();
    if (fonts.main) TTF_CloseFont(fonts.main);
    if (fonts.small) TTF_CloseFont(fonts.small);
    if (fonts.title) TTF_CloseFont(fonts.title);
    if (fonts.artist) TTF_CloseFont(fonts.artist);
    if (fonts.album) TTF_CloseFont(fonts.album);
    if (fonts.time_elapsed) TTF_CloseFont(fonts.time_elapsed);
    if (fonts.time_dur) TTF_CloseFont(fonts.time_dur);
    if (fonts.xs) TTF_CloseFont(fonts.xs);
    if (fonts.large) TTF_CloseFont(fonts.large);
    fonts = {};

    if (g_keytone_loaded) { ma_sound_uninit(&g_keytone); g_keytone_loaded = false; }
    if (g_audio_engine_ok) { ma_engine_uninit(&g_audio_engine); g_audio_engine_ok = false; }
}

SDL_Texture* get_image(const std::string& name) {
    auto it = images.find(name);
    return it == images.end() ? nullptr : it->second;
}

void play_sfx(const std::string& name) {
    if (name != "nav") return;
    if (!g_keytone_loaded) return;
    ma_sound_seek_to_pcm_frame(&g_keytone, 0);
    ma_sound_start(&g_keytone);
}

} // namespace assets
