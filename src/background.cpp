// Direct port of background.lua. No blur shader (constraint). Wallpaper drawn plain.
#include "background.h"
#include "common.h"
#include "render.h"
#include "settings.h"
#include "theme.h"
#include "utils.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace background {

struct Particle { float x, y, size, speed, alpha; };
static std::vector<Particle> g_particles;
static int g_screen_w = 0, g_screen_h = 0;
static float g_speed = 1.0f, g_target_speed = 1.0f;
static bool g_custom_bg_enabled = false;
static SDL_Texture* g_custom_bg_image = nullptr;
static std::string g_custom_bg_path;
static bool g_blur_enabled = true;       // not used (no shader)
static bool g_tint_enabled = true;
static int  g_brightness = 1; // 1 no change, 2 brighter, 3 darker

static std::mt19937& rng() { static std::mt19937 r{std::random_device{}()}; return r; }
static float frand() { return std::uniform_real_distribution<float>(0, 1)(rng()); }
static int   irand(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng()); }

static SDL_Texture* load_wallpaper(const std::string& path) {
    if (g_custom_bg_image) { SDL_DestroyTexture(g_custom_bg_image); g_custom_bg_image = nullptr; }
    std::string p = path;
    if (p.empty()) p = "assets/background/bg.jpg";
    SDL_Texture* t = IMG_LoadTexture(g_app.renderer, p.c_str());
    if (t) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    return t;
}

void init() {
    g_screen_w = g_app.screen_w;
    g_screen_h = g_app.screen_h;
    g_particles.clear();
    g_particles.reserve(50);
    for (int i = 0; i < 50; ++i) {
        Particle p;
        p.x = frand() * g_screen_w;
        p.y = frand() * g_screen_h;
        p.size = (float)irand(1, 3);
        p.speed = 10 + frand() * 20;
        p.alpha = 0.4f + frand() * 0.4f;
        g_particles.push_back(p);
    }
}

void update(float dt, bool is_paused) {
    g_target_speed = is_paused ? 0.15f : 1.0f;
    float t = std::min(1.0f, dt * 2.0f);
    g_speed = utils::lerp(g_speed, g_target_speed, t);

    if (settings::show_particles) {
        float pdt = std::min(dt, 1.0f / 30.0f);
        for (auto& p : g_particles) {
            p.x += p.speed * pdt * g_speed * 0.5f;
            if (p.x > g_screen_w) {
                p.x = -10;
                p.y = frand() * g_screen_h;
            }
        }
    }
}

void set_custom_bg(bool enabled) {
    g_custom_bg_enabled = enabled;
    if (g_custom_bg_enabled) g_custom_bg_image = load_wallpaper(g_custom_bg_path);
    else if (g_custom_bg_image) { SDL_DestroyTexture(g_custom_bg_image); g_custom_bg_image = nullptr; }
}

void set_custom_bg_path(const std::string& path) {
    g_custom_bg_path = path;
    if (g_custom_bg_enabled) g_custom_bg_image = load_wallpaper(g_custom_bg_path);
}

void set_wallpaper_blur(bool e) { g_blur_enabled = e; }
void set_wallpaper_tint(bool e) { g_tint_enabled = e; }
void set_wallpaper_brightness(int m) { g_brightness = std::max(1, std::min(3, m)); }

bool has_custom_wallpaper() { return g_custom_bg_enabled && g_custom_bg_image; }

static void draw_psp_waves() {
    int w = g_screen_w, h = g_screen_h;
    double t = SDL_GetTicksNS() / 1.0e9;
    for (int layer = 1; layer <= 2; ++layer) {
        float opacity = (layer == 1) ? 0.15f : 0.10f;
        float speed_mult = (layer == 1) ? 0.4f : 0.2f;
        float freq_mult = (layer == 1) ? 1.0f : 0.6f;
        float amplitude = (layer == 1) ? h * 0.1f : h * 0.15f;
        float base_y = h * 0.60f;

        Color color{theme::accent.r, theme::accent.g, theme::accent.b, opacity};
        Color fade{theme::accent.r, theme::accent.g, theme::accent.b, 0};

        const int segments = 40;
        std::vector<SDL_Vertex> verts;
        verts.reserve((segments + 1) * 2);

        for (int i = 0; i <= segments; ++i) {
            float x = (i / (float)segments) * w;
            float y = base_y +
                std::sin(i * 0.2f * freq_mult + t * speed_mult) * amplitude +
                std::sin(i * 0.1f * freq_mult - t * speed_mult * 0.8f) * (amplitude * 0.4f);
            SDL_Vertex top{}, bot{};
            top.position = {x, y};
            top.color = {color.r, color.g, color.b, color.a};
            top.tex_coord = {0, 0};
            bot.position = {x, y + 250};
            bot.color = {fade.r, fade.g, fade.b, fade.a};
            bot.tex_coord = {0, 0};
            verts.push_back(top);
            verts.push_back(bot);
        }

        // Build triangle strip as triangle list indices
        std::vector<int> idx;
        for (int i = 0; i + 1 < (int)verts.size() / 2; ++i) {
            int a = i * 2;
            int b = i * 2 + 1;
            int c = (i + 1) * 2;
            int d = (i + 1) * 2 + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(c);
            idx.push_back(b); idx.push_back(d); idx.push_back(c);
        }
        render::render_geometry(verts, idx, nullptr);
    }
}

static void draw_gradient() {
    Color bg = theme::colors.background;
    Color acc = theme::accent;
    float tint = (theme::current_mode == "Light") ? 0.3f : 0.15f;
    Color c1{bg.r, bg.g, bg.b, 1};
    Color c2{
        bg.r * (1 - tint) + acc.r * tint,
        bg.g * (1 - tint) + acc.g * tint,
        bg.b * (1 - tint) + acc.b * tint, 1};
    SDL_Vertex v[4];
    v[0] = {{0, 0}, {c1.r, c1.g, c1.b, 1}, {0, 0}};
    v[1] = {{(float)g_screen_w, 0}, {c1.r, c1.g, c1.b, 1}, {0, 0}};
    v[2] = {{(float)g_screen_w, (float)g_screen_h}, {c2.r, c2.g, c2.b, 1}, {0, 0}};
    v[3] = {{0, (float)g_screen_h}, {c2.r, c2.g, c2.b, 1}, {0, 0}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(g_app.renderer, nullptr, v, 4, idx, 6);
}

static void draw_waveform_visualizer(const float* pcm, int count, int pos) {
    if (!pcm || count <= 0) return;
    int w = g_screen_w, h = g_screen_h;
    const int samples = 120;
    float amplitude = h * 0.15f;
    int window = 1024;
    float rms = 0;
    for (int i = 0; i < window; i += 32) {
        int idx = std::min(count - 1, pos + i);
        float s = pcm[idx];
        rms += s * s;
    }
    rms = std::sqrt(rms / (window / 32.0f));
    float reactive_amp = amplitude * (0.2f + rms * 2.5f);

    double t = SDL_GetTicksNS() / 1.0e9;
    for (int layer = 1; layer <= 3; ++layer) {
        float opacity = (layer == 1) ? 0.8f : (layer == 2) ? 0.4f : 0.2f;
        float layer_speed = (layer == 1) ? 1.0f : (layer == 2) ? 0.7f : 0.4f;
        float layer_freq = (layer == 1) ? 1.0f : (layer == 2) ? 0.6f : 0.3f;
        Color c{theme::accent.r, theme::accent.g, theme::accent.b, opacity};
        render::set_color(c);
        std::vector<float> pts;
        pts.reserve((samples + 1) * 2);
        for (int i = 0; i <= samples; ++i) {
            float x = (i / (float)samples) * w;
            int sample_offset = i * 10;
            int idx = std::min(count - 1, pos + sample_offset);
            float audio_sample = pcm[idx];
            float y_offset = std::sin(i * 0.1f * layer_freq + t * layer_speed) * 10;
            float y = (h * 0.6f) + (audio_sample * reactive_amp) + y_offset;
            pts.push_back(x);
            pts.push_back(y);
        }
        render::polyline(pts);
    }
}

void draw_with_music_state(bool music_active, bool music_playing, bool music_paused, const std::string& visualizer_mode, const float* pcm, int pcm_count, int pcm_pos) {
    (void)music_paused;
    draw_gradient();

    if (g_custom_bg_enabled && g_custom_bg_image) {
        float iw, ih;
        SDL_GetTextureSize(g_custom_bg_image, &iw, &ih);
        float scale = std::max(g_screen_w / iw, g_screen_h / ih);
        float r = 1, g = 1, b = 1;
        if (g_tint_enabled) {
            Color bg = theme::colors.background;
            if (theme::current_mode == "Light") {
                r = bg.r + 0.4f; g = bg.g + 0.4f; b = bg.b + 0.4f;
            } else {
                r = bg.r * 3.0f; g = bg.g * 3.0f; b = bg.b * 3.0f;
            }
        }
        if (g_brightness == 2) { r *= 1.5f; g *= 1.5f; b *= 1.5f; }
        else if (g_brightness == 3) { r *= 0.5f; g *= 0.5f; b *= 0.5f; }
        SDL_SetTextureColorModFloat(g_custom_bg_image, std::min(r, 1.0f), std::min(g, 1.0f), std::min(b, 1.0f));
        SDL_SetTextureAlphaModFloat(g_custom_bg_image, 1.0f);
        render::draw_texture_scaled(g_custom_bg_image, g_screen_w * 0.5f, g_screen_h * 0.5f, scale, 0.5f, 0.5f);
    }

    if (!music_active && !g_custom_bg_enabled) draw_psp_waves();

    if (music_playing && visualizer_mode == "wave") {
        draw_waveform_visualizer(pcm, pcm_count, pcm_pos);
    } else if (music_playing && visualizer_mode == "bars" && pcm && pcm_count > 0) {
        const int bars = 42, gap = 4;
        int W = g_screen_w, H = g_screen_h;
        float bottom_y = (float)H;
        float max_h = H * 0.28f;
        float total_width = (float)W;
        float bar_w = (total_width - (bars - 1) * gap) / bars;
        float start_x = (W - total_width) * 0.5f;
        int total_samples = pcm_count;
        int current_sample = std::max(0, pcm_pos);
        float paused_alpha = music_paused ? 0.35f : 1.0f;
        for (int i = 0; i < bars; ++i) {
            float progress = (float)i / std::max(1, bars - 1);
            int window_size = 480;
            int window_start = current_sample + (int)(progress * 8192);
            float energy = 0;
            for (int j = 0; j < window_size; j += 24) {
                int idx = std::min(total_samples - 1, window_start + j);
                float sample = pcm[idx];
                energy += sample * sample;
            }
            float rms = std::sqrt(energy / (window_size / 24.0f));
            float strength = std::min(1.0f, rms * 5.2f);
            float bar_h = 8 + (strength * max_h);
            float x = start_x + i * (bar_w + gap);
            float y = bottom_y - bar_h;
            float alpha = (0.25f + strength * 0.85f) * paused_alpha;
            render::set_color(Color{theme::accent.r, theme::accent.g, theme::accent.b, alpha * 0.95f});
            render::rect_fill_rounded(x, y, bar_w, bar_h, 2);
            render::set_color(Color{1, 1, 1, alpha * 0.12f});
            render::rect_fill_rounded(x, y, bar_w, std::max(2.0f, bar_h * 0.16f), 2);
        }
    }

    if (settings::show_particles) {
        for (auto& p : g_particles) {
            Color c{theme::accent.r, theme::accent.g, theme::accent.b, p.alpha * 0.5f};
            render::set_color(c);
            render::circle_fill(p.x, p.y, p.size);
        }
    }
}

} // namespace background
