#pragma once
#include "common.h"
#include <SDL3/SDL.h>
#include <vector>

namespace render {

void set_color(const Color& c);
void set_color(const Color& c, float alpha_mult);
void clear(const Color& c);
void rect_fill(float x, float y, float w, float h);
void rect_line(float x, float y, float w, float h, float thickness = 1.0f);
void rect_fill_rounded(float x, float y, float w, float h, float radius);
void rect_line_rounded(float x, float y, float w, float h, float radius, float thickness = 1.0f);
void circle_fill(float cx, float cy, float r);
void line(float x1, float y1, float x2, float y2, float thickness = 1.0f);
void polyline(const std::vector<float>& xy);

// triangle fan with per-vertex colors (positions in screen space). count = num vertices.
void render_geometry(const std::vector<SDL_Vertex>& verts, const std::vector<int>& indices = {}, SDL_Texture* tex = nullptr);

void push_translate(float dx, float dy);
void pop_translate();
void get_translate(float& tx, float& ty);

void set_clip(int x, int y, int w, int h);
void clear_clip();

void draw_texture(SDL_Texture* tex, float dst_x, float dst_y, float dst_w, float dst_h);
void draw_texture_rotated(SDL_Texture* tex, float cx, float cy, float w, float h, float angle_rad);

// scaled draw with origin (cx,cy) at given anchor (default center). scale uniform.
void draw_texture_scaled(SDL_Texture* tex, float anchor_x, float anchor_y, float scale, float origin_u = 0.5f, float origin_v = 0.5f);

} // namespace render
