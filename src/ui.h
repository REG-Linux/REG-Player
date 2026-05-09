#pragma once
#include "common.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>

namespace ui {

struct Marquee {
    int offset = 0;
    float timer = 0;
    std::string phase = "pause_start";
    int max_width = 200;
    float speed = 40.0f;
    float pause_start = 3.0f;
    float pause_end = 1.5f;
};

Marquee new_marquee(int max_width, float speed = 40.0f, float pause_start = 1.5f, float pause_end = 1.0f);
void update_marquee(Marquee& m, float dt, int text_width);
void draw_marquee(Marquee& m, const std::string& text, float x, float y,
                  TTF_Font* font, const Color& color, float abs_x, float abs_y,
                  const Color* glow_color = nullptr);

void draw_progress_bar(float x, float y, float w, float h, float progress, const Color& color, const Color& bg_color);
void draw_indexing_popup(const std::string& progress_text, bool final_message);
void draw_icon(SDL_Texture* icon, float x, float y, float size, const Color& color, float alpha, SDL_Texture* thumbnail = nullptr);
void draw_glow_text(const std::string& text, float x, float y, TTF_Font* font, const Color& color,
                    const Color* glow_color, int limit = 0, const std::string& align = "");
void draw_glow_icon(SDL_Texture* icon, float x, float y, float size, const Color& color, float alpha,
                    const Color* glow_color = nullptr, SDL_Texture* thumbnail = nullptr);

void show_toast(const std::string& text, const std::string& icon_name, const std::string& position);
void show_volume_toast(int volume);
void show_brightness_toast(int brightness);
void update_toasts(float dt);
void draw_toasts();

// Helper for plain text
void draw_text(const std::string& text, float x, float y, TTF_Font* font, const Color& color);
void draw_text_aligned(const std::string& text, float x, float y, int width, TTF_Font* font, const Color& color, const std::string& align);
int text_width(TTF_Font* font, const std::string& s);
int text_height(TTF_Font* font);

} // namespace ui
