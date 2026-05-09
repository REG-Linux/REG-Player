// Embedded libmpv via render API (SW backend).
// mpv decodes to CPU buffer, we upload to SDL_Texture and composite via SDL_Renderer.
// Watch-later resume + history record preserved (matches Lua original semantics).
#include "player.h"
#include "common.h"
#include "history.h"
#include "video_manager.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mpv/client.h>
#include <mpv/render.h>
#include <thread>
#include <vector>

namespace player {

bool needs_refresh = false;

static std::atomic<bool> g_render_pending{false};
static std::atomic<bool> g_event_pending{false};

static void on_render_update(void* /*ctx*/) { g_render_pending = true; }
static void on_mpv_events(void* /*ctx*/) { g_event_pending = true; }

static void play_paths(const std::vector<std::string>& paths, bool resume) {
    if (paths.empty()) return;
    history::add(paths[0]);

    std::string watch_dir = video_manager::watch_later_dir();

    mpv_handle* mpv = mpv_create();
    if (!mpv) { std::fprintf(stderr, "mpv_create failed\n"); return; }

    // Options
    mpv_set_option_string(mpv, "watch-later-directory", watch_dir.c_str());
    mpv_set_option_string(mpv, "save-position-on-quit", "yes");
    mpv_set_option_string(mpv, "write-filename-in-watch-later-config", "yes");
    mpv_set_option_string(mpv, "resume-playback", resume ? "yes" : "no");
    mpv_set_option_string(mpv, "hwdec", "auto-safe");
    mpv_set_option_string(mpv, "vo", "libmpv");
    mpv_set_option_string(mpv, "ytdl", "no");
    mpv_set_option_string(mpv, "input-default-bindings", "no");
    mpv_set_option_string(mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(mpv, "osc", "no");

    if (mpv_initialize(mpv) < 0) {
        std::fprintf(stderr, "mpv_initialize failed\n");
        mpv_destroy(mpv);
        return;
    }

    // SW render context
    mpv_render_param init_params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_SW},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    mpv_render_context* rctx = nullptr;
    if (mpv_render_context_create(&rctx, mpv, init_params) < 0) {
        std::fprintf(stderr, "mpv_render_context_create failed\n");
        mpv_destroy(mpv);
        return;
    }
    g_render_pending = false;
    g_event_pending  = false;
    mpv_render_context_set_update_callback(rctx, on_render_update, nullptr);
    mpv_set_wakeup_callback(mpv, on_mpv_events, nullptr);

    // Load files (each loadfile with append-play stacks the playlist)
    for (size_t i = 0; i < paths.size(); ++i) {
        const char* mode = (i == 0) ? "replace" : "append-play";
        const char* cmd[] = {"loadfile", paths[i].c_str(), mode, nullptr};
        mpv_command_async(mpv, 0, cmd);
    }

    // Texture for video output
    int sw = g_app.screen_w, sh = g_app.screen_h;
    SDL_Texture* video_tex = SDL_CreateTexture(g_app.renderer, SDL_PIXELFORMAT_RGBA32,
                                               SDL_TEXTUREACCESS_STREAMING, sw, sh);
    if (!video_tex) {
        std::fprintf(stderr, "video texture create failed: %s\n", SDL_GetError());
        mpv_render_context_free(rctx);
        mpv_destroy(mpv);
        return;
    }
    SDL_SetTextureScaleMode(video_tex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(video_tex, SDL_BLENDMODE_NONE);
    std::vector<uint8_t> framebuf((size_t)sw * sh * 4, 0);

    bool quit = false;
    bool eof_seen = false;
    bool input_locked_until_release = false;
    Uint64 last_input = 0;

    auto pump_events = [&]() {
        while (true) {
            mpv_event* ev = mpv_wait_event(mpv, 0);
            if (!ev || ev->event_id == MPV_EVENT_NONE) break;
            switch (ev->event_id) {
                case MPV_EVENT_END_FILE: {
                    auto* end = (mpv_event_end_file*)ev->data;
                    if (end && (end->reason == MPV_END_FILE_REASON_EOF ||
                                end->reason == MPV_END_FILE_REASON_QUIT ||
                                end->reason == MPV_END_FILE_REASON_ERROR)) {
                        eof_seen = true;
                    }
                    break;
                }
                case MPV_EVENT_IDLE:
                    if (eof_seen) quit = true;
                    break;
                case MPV_EVENT_SHUTDOWN:
                    quit = true;
                    break;
                default: break;
            }
        }
    };

    auto send_cmd = [&](std::initializer_list<const char*> args) {
        std::vector<const char*> v(args);
        v.push_back(nullptr);
        mpv_command_async(mpv, 0, v.data());
    };

    auto recreate_tex = [&](int new_w, int new_h) {
        if (new_w <= 0 || new_h <= 0) return;
        if (new_w == sw && new_h == sh) return;
        if (video_tex) SDL_DestroyTexture(video_tex);
        sw = new_w; sh = new_h;
        video_tex = SDL_CreateTexture(g_app.renderer, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STREAMING, sw, sh);
        if (video_tex) {
            SDL_SetTextureScaleMode(video_tex, SDL_SCALEMODE_LINEAR);
            SDL_SetTextureBlendMode(video_tex, SDL_BLENDMODE_NONE);
        }
        framebuf.assign((size_t)sw * sh * 4, 0);
    };

    while (!quit && !g_app.quit_requested) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    g_app.quit_requested = true; quit = true; break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_RESIZED: {
                    int nw = 0, nh = 0;
                    SDL_GetWindowSizeInPixels(g_app.window, &nw, &nh);
                    g_app.screen_w = nw; g_app.screen_h = nh;
                    recreate_tex(nw, nh);
                    break;
                }
                case SDL_EVENT_KEY_DOWN: {
                    Uint64 now = SDL_GetTicksNS();
                    if (now - last_input < 30000000) break; // 30ms debounce
                    last_input = now;
                    SDL_Keycode k = e.key.key;
                    if (k == SDLK_SPACE || k == SDLK_RETURN) send_cmd({"cycle", "pause"});
                    else if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE) { quit = true; }
                    else if (k == SDLK_LEFT)  send_cmd({"seek", "-5", "relative"});
                    else if (k == SDLK_RIGHT) send_cmd({"seek", "5",  "relative"});
                    else if (k == SDLK_UP)    send_cmd({"seek", "60", "relative"});
                    else if (k == SDLK_DOWN)  send_cmd({"seek", "-60", "relative"});
                    else if (k == SDLK_X)     send_cmd({"cycle", "mute"});
                    else if (k == SDLK_PAGEUP) send_cmd({"playlist-prev"});
                    else if (k == SDLK_PAGEDOWN) send_cmd({"playlist-next"});
                    else if (k == SDLK_L)     send_cmd({"cycle", "sub-visibility"});
                    else if (k == SDLK_E)     send_cmd({"cycle", "sub"});
                    break;
                }
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                    Uint64 now = SDL_GetTicksNS();
                    if (now - last_input < 30000000) break;
                    last_input = now;
                    auto b = (SDL_GamepadButton)e.gbutton.button;
                    if (b == SDL_GAMEPAD_BUTTON_SOUTH || b == SDL_GAMEPAD_BUTTON_START) send_cmd({"cycle", "pause"});
                    else if (b == SDL_GAMEPAD_BUTTON_BACK || b == SDL_GAMEPAD_BUTTON_EAST) { quit = true; }
                    else if (b == SDL_GAMEPAD_BUTTON_DPAD_LEFT)  send_cmd({"seek", "-5",  "relative"});
                    else if (b == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) send_cmd({"seek", "5",   "relative"});
                    else if (b == SDL_GAMEPAD_BUTTON_DPAD_UP)    send_cmd({"seek", "60",  "relative"});
                    else if (b == SDL_GAMEPAD_BUTTON_DPAD_DOWN)  send_cmd({"seek", "-60", "relative"});
                    else if (b == SDL_GAMEPAD_BUTTON_WEST)        send_cmd({"cycle", "mute"});
                    else if (b == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)  send_cmd({"playlist-prev"});
                    else if (b == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) send_cmd({"playlist-next"});
                    break;
                }
            }
        }
        if (input_locked_until_release) {} // placeholder

        if (g_event_pending.exchange(false)) pump_events();

        if (g_render_pending.exchange(false)) {
            int size_arr[2] = {sw, sh};
            size_t stride = (size_t)sw * 4;
            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_SW_SIZE,    size_arr},
                {MPV_RENDER_PARAM_SW_FORMAT,  (void*)"rgb0"},
                {MPV_RENDER_PARAM_SW_STRIDE,  &stride},
                {MPV_RENDER_PARAM_SW_POINTER, framebuf.data()},
                {MPV_RENDER_PARAM_INVALID,    nullptr}
            };
            mpv_render_context_render(rctx, params);
            SDL_UpdateTexture(video_tex, nullptr, framebuf.data(), (int)stride);
        }

        SDL_SetRenderDrawColor(g_app.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_app.renderer);
        SDL_FRect dst{0, 0, (float)sw, (float)sh};
        SDL_RenderTexture(g_app.renderer, video_tex, nullptr, &dst);
        SDL_RenderPresent(g_app.renderer);

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    // Send quit so save-position-on-quit fires watch-later write.
    const char* qcmd[] = {"quit", nullptr};
    mpv_command(mpv, qcmd);
    // Drain final events.
    for (int i = 0; i < 50; ++i) {
        mpv_event* ev = mpv_wait_event(mpv, 0.01);
        if (!ev || ev->event_id == MPV_EVENT_SHUTDOWN) break;
    }

    SDL_DestroyTexture(video_tex);
    mpv_render_context_free(rctx);
    mpv_destroy(mpv);
    needs_refresh = false; // no display refresh hack required with embed
}

void play_video(const std::string& filepath, bool resume) {
    play_paths({filepath}, resume);
}

void play_video(const std::vector<std::string>& paths, bool resume) {
    play_paths(paths, resume);
}

} // namespace player
