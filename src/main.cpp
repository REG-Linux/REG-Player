// main.cpp — port of main.lua. Same frame orchestration shape.
#include "common.h"
#include "assets.h"
#include "background.h"
#include "browser.h"
#include "categories.h"
#include "gamepad.h"
#include "history.h"
#include "image_view.h"
#include "image_viewer.h"
#include "indexing.h"
#include "music_player.h"
#include "music_view.h"
#include "player.h"
#include "render.h"
#include "settings.h"
#include "settings_view.h"
#include "system.h"
#include "theme.h"
#include "ui.h"
#include "utils.h"
#include "video_manager.h"
#include "xmb.h"
#include "xmb_draw.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cstdio>
#include <random>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_set>

static const float LAUNCH_STATUS_DURATION = 3.0f;
static std::string g_launch_status_message;
static float g_launch_status_timer = 0;

static int g_battery_percentage = -1;
static bool g_is_charging = false;
static float g_battery_timer = 0;
static const float BATTERY_UPDATE_INTERVAL = 10.0f;

static int g_last_volume = -1;
static int g_last_brightness = -1;
static float g_ui_timer = 0;
static const float UI_CHECK_INTERVAL = 0.1f;

static bool g_was_music_active = false;
static bool g_index_complete_pending = false;
static bool g_did_kickoff_scan = false;

static void build_valid_media_paths(std::unordered_set<std::string>& out) {
    std::lock_guard<std::mutex> lock(indexing::data_mutex);
    for (auto& kv : indexing::data.music_files) out.insert(kv.first);
    for (auto& kv : indexing::data.photos)      out.insert(kv.first);
    for (auto& v  : indexing::data.videos)       out.insert(v);
}

static void cleanup_stale_media_state() {
    std::unordered_set<std::string> valid;
    build_valid_media_paths(valid);
    video_manager::prune_stale_state(valid);
    history::prune_missing(valid);
}

static bool init_window() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");

    SDL_DisplayID dpy = SDL_GetPrimaryDisplay();
    int w = 1280, h = 720;
    if (dpy) {
        const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(dpy);
        if (m) { w = m->w; h = m->h; }
    }
    g_app.window = SDL_CreateWindow("REG-Player", w, h, SDL_WINDOW_FULLSCREEN);
    if (!g_app.window) {
        std::fprintf(stderr, "Window create failed: %s\n", SDL_GetError());
        return false;
    }
    g_app.renderer = SDL_CreateRenderer(g_app.window, nullptr);
    if (!g_app.renderer) {
        std::fprintf(stderr, "Renderer create failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(g_app.renderer, SDL_BLENDMODE_BLEND);
    SDL_GetWindowSizeInPixels(g_app.window, &g_app.screen_w, &g_app.screen_h);
    return true;
}

static void shutdown_window() {
    if (g_app.renderer) SDL_DestroyRenderer(g_app.renderer);
    if (g_app.window)   SDL_DestroyWindow(g_app.window);
    SDL_Quit();
}

static void love_load() {
    std::srand((unsigned)SDL_GetTicksNS());
    assets::load();
    background::init();
    music_player::init();
    music_view::init();
    image_viewer::init();
    settings::load();

    bool force_reindex = settings::consume_reindex_request();
    bool has_existing_index = false;
    if (!force_reindex) has_existing_index = indexing::load();

    std::string photo_dir = settings::get_option("photo_dir") ? settings::get_option("photo_dir")->str_value : "";
    std::string music_dir = settings::get_option("music_dir") ? settings::get_option("music_dir")->str_value : "";
    std::string video_dir = settings::get_option("video_dir") ? settings::get_option("video_dir")->str_value : "";
    if (photo_dir.empty()) photo_dir = "/userdata/medias/photos";
    if (music_dir.empty()) music_dir = "/userdata/medias/music";
    if (video_dir.empty()) video_dir = "/userdata/medias/videos";

    // If /userdata/medias absent, fall back to ~/Music ~/Videos ~/Pictures
    auto path_exists = [](const std::string& p) {
        struct stat st; return ::stat(p.c_str(), &st) == 0;
    };
    if (!path_exists("/userdata/medias")) {
        const char* home = std::getenv("HOME");
        if (home) {
            std::string h = home;
            if (path_exists(h + "/Music"))    music_dir = h + "/Music";
            if (path_exists(h + "/Videos"))   video_dir = h + "/Videos";
            if (path_exists(h + "/Pictures")) photo_dir = h + "/Pictures";
        }
    }

    bool empty_index;
    {
        std::lock_guard<std::mutex> lock(indexing::data_mutex);
        empty_index = indexing::data.music_files.empty() && indexing::data.photos.empty();
    }

    if (force_reindex || !has_existing_index || empty_index) {
        indexing::scan_async(photo_dir, music_dir, video_dir);
    } else {
        indexing::scan_for_new_async(photo_dir, music_dir, video_dir);
    }
    g_did_kickoff_scan = true;
    g_index_complete_pending = true;

    history::load();
    video_manager::load_watched();

    g_battery_percentage = system_info::get_battery_percentage().value_or(-1);
    g_is_charging        = system_info::is_charging();
    g_last_volume        = system_info::get_volume().value_or(-1);
    g_last_brightness    = system_info::get_brightness().value_or(-1);
}

static void love_update(float dt) {
    bool is_paused = false;
    if (music_player::s.active) {
        if (!g_was_music_active) music_view::on_music_opened();
        music_player::update(dt);
        if (music_player::s.active) music_view::update(dt);
        is_paused = music_player::s.paused;
    } else if (image_viewer::s.active) {
        if (g_was_music_active) music_view::on_music_closed();
        image_viewer::update(dt);
        is_paused = true;
    } else if (indexing::is_scanning.load()) {
        if (g_was_music_active) music_view::on_music_closed();
        // background thread runs scan; check for completion next frame
        return;
    } else if (g_launch_status_timer > 0) {
        if (g_was_music_active) music_view::on_music_closed();
        g_launch_status_timer = std::max(0.0f, g_launch_status_timer - dt);
        if (g_launch_status_timer == 0) g_launch_status_message.clear();
        return;
    } else {
        if (g_was_music_active) music_view::on_music_closed();
        xmb::update(dt);
    }
    g_was_music_active = music_player::s.active;

    g_battery_timer += dt;
    if (g_battery_timer >= BATTERY_UPDATE_INTERVAL) {
        g_battery_timer = 0;
        g_battery_percentage = system_info::get_battery_percentage().value_or(-1);
        g_is_charging        = system_info::is_charging();
    }

    g_ui_timer += dt;
    if (g_ui_timer >= UI_CHECK_INTERVAL) {
        g_ui_timer = 0;
        int cur_v = system_info::get_volume().value_or(-1);
        if (settings::vol_bright_enabled && g_last_volume >= 0 && cur_v >= 0 && cur_v != g_last_volume) {
            ui::show_volume_toast(cur_v);
        }
        g_last_volume = cur_v;
        int cur_b = system_info::get_brightness().value_or(-1);
        if (settings::vol_bright_enabled && g_last_brightness >= 0 && cur_b >= 0 && cur_b != g_last_brightness) {
            ui::show_brightness_toast(cur_b);
        }
        g_last_brightness = cur_b;
    }

    background::update(dt, is_paused);
    ui::update_toasts(dt);
}

static void love_draw() {
    int sw = g_app.screen_w;
    (void)g_app.screen_h;
    render::clear(theme::colors.background);

    background::draw_with_music_state(
        music_player::s.active, music_player::s.playing, music_player::s.paused,
        music_player::s.visualizer_mode,
        music_player::pcm(), music_player::pcm_count(), music_player::pcm_pos());

    if (music_player::s.active) {
        music_view::draw();
    } else if (image_viewer::s.active) {
        image_view::draw();
    } else if (indexing::is_scanning.load() || g_launch_status_timer > 0) {
        std::string msg = (g_launch_status_timer > 0) ? g_launch_status_message : indexing::current_progress();
        ui::draw_indexing_popup(msg, g_launch_status_timer > 0);
    } else {
        xmb_draw::draw();

        // Clock + battery
        Color tc{theme::text.r, theme::text.g, theme::text.b, 0.8f};
        time_t now = time(nullptr);
        struct tm tm_;
        localtime_r(&now, &tm_);
        char clock[16];
        std::snprintf(clock, sizeof(clock), "%02d:%02d", tm_.tm_hour, tm_.tm_min);
        ui::draw_text(clock, 20, 20, assets::fonts.small, tc);

        if (g_battery_percentage >= 0) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d%%", g_battery_percentage);
            int bw = ui::text_width(assets::fonts.small, buf);
            SDL_Texture* icon = assets::get_image(g_is_charging ? "battery_charge" : "battery");
            if (icon) {
                int icon_h = ui::text_height(assets::fonts.small);
                float iw, ih;
                SDL_GetTextureSize(icon, &iw, &ih);
                float scale = icon_h / ih;
                float iconw = iw * scale;
                float x = sw - bw - iconw - 25;
                SDL_SetTextureColorModFloat(icon, theme::text.r, theme::text.g, theme::text.b);
                SDL_SetTextureAlphaModFloat(icon, 0.8f);
                render::draw_texture_scaled(icon, x, 20 + icon_h * 0.5f, scale, 0, 0.5f);
            }
            ui::draw_text(buf, sw - bw - 20, 20, assets::fonts.small, tc);
        }
    }

    if (!(music_player::s.active && music_view::is_display_sleeping())) ui::draw_toasts();

    SDL_RenderPresent(g_app.renderer);
}

static void love_keypressed(const std::string& key) {
    if (music_player::s.active) { music_view::keypressed(key); return; }
    if (image_viewer::s.active) { image_view::keypressed(key); return; }
    xmb::keypressed(key);
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            std::printf(
                "Usage: regplayer [--help] [--version]\n"
                "\n"
                "XMB-inspired media suite for Linux handhelds.\n"
                "Scans /userdata/medias by default. Override per-type via Settings.\n"
                "\n"
                "Config:  $XDG_CONFIG_HOME/regplayer (default ~/.config/regplayer)\n"
                "Cache:   $XDG_CACHE_HOME/regplayer  (default ~/.cache/regplayer)\n"
                "Assets:  $REG_ASSETS_DIR override, otherwise /usr/local/share/regplayer/assets\n"
            );
            return 0;
        }
        if (a == "--version" || a == "-v") {
            std::printf("regplayer v0.1\n");
            return 0;
        }
    }
    if (!init_window()) return 1;

    if (!TTF_Init()) {
        std::fprintf(stderr, "TTF_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    gamepad::init();
    love_load();
    xmb::refresh_browser();

    Uint64 last = SDL_GetTicksNS();
    while (!g_app.quit_requested) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            gamepad::on_event(e);
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    g_app.quit_requested = true;
                    break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_RESIZED:
                    SDL_GetWindowSizeInPixels(g_app.window, &g_app.screen_w, &g_app.screen_h);
                    break;
                case SDL_EVENT_KEY_DOWN: {
                    if (e.key.repeat) break; // Ignore OS auto-repeat — our xmb::update has its own
                    std::string k = gamepad::keycode_to_logical(e.key.key);
                    if (!k.empty()) love_keypressed(k);
                    if (e.key.key == SDLK_ESCAPE) g_app.quit_requested = true;
                    break;
                }
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                    std::string k = gamepad::gamepad_button_to_logical((SDL_GamepadButton)e.gbutton.button);
                    if (!k.empty()) love_keypressed(k);
                    break;
                }
            }
        }

        // Detect indexing completion
        bool scanning = indexing::is_scanning.load();
        if (g_index_complete_pending && !scanning) {
            cleanup_stale_media_state();
            xmb::refresh_browser();
            // First-run hint when nothing was indexed.
            bool empty_total;
            {
                std::lock_guard<std::mutex> lock(indexing::data_mutex);
                empty_total = indexing::data.music_files.empty() &&
                              indexing::data.photos.empty() &&
                              indexing::data.videos.empty();
            }
            if (empty_total) {
                g_launch_status_message = "No media found. Set directories in Settings.";
                g_launch_status_timer = LAUNCH_STATUS_DURATION + 2;
            } else {
                g_launch_status_message = "Indexing Complete";
                g_launch_status_timer = LAUNCH_STATUS_DURATION;
            }
            g_index_complete_pending = false;
        }

        Uint64 now = SDL_GetTicksNS();
        float dt = (now - last) / 1.0e9f;
        if (dt > 0.1f) dt = 0.1f;
        last = now;

        love_update(dt);
        love_draw();
    }

    indexing::cancel_scan = true;
    indexing::wait_finish();
    music_player::close();
    assets::unload();
    TTF_Quit();
    gamepad::shutdown();
    shutdown_window();

    if (g_app.restart_requested) {
        // Re-exec self to perform full reload + reindex.
        execv(argv[0], argv);
    }
    return 0;
}
