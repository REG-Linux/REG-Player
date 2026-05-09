#include "render.h"
#include "common.h"
#include <algorithm>
#include <cmath>
#include <vector>

AppContext g_app;

namespace render {

static std::vector<std::pair<float,float>> g_xform_stack;
static float g_tx = 0, g_ty = 0;

void set_color(const Color& c) {
    SDL_SetRenderDrawColorFloat(g_app.renderer, c.r, c.g, c.b, c.a);
}
void set_color(const Color& c, float alpha_mult) {
    SDL_SetRenderDrawColorFloat(g_app.renderer, c.r, c.g, c.b, c.a * alpha_mult);
}

void clear(const Color& c) {
    SDL_SetRenderDrawColorFloat(g_app.renderer, c.r, c.g, c.b, 1.0f);
    SDL_RenderClear(g_app.renderer);
}

void rect_fill(float x, float y, float w, float h) {
    SDL_FRect r{x + g_tx, y + g_ty, w, h};
    SDL_RenderFillRect(g_app.renderer, &r);
}

void rect_line(float x, float y, float w, float h, float thickness) {
    if (thickness <= 1.0f) {
        SDL_FRect r{x + g_tx, y + g_ty, w, h};
        SDL_RenderRect(g_app.renderer, &r);
        return;
    }
    rect_fill(x, y, w, thickness);
    rect_fill(x, y + h - thickness, w, thickness);
    rect_fill(x, y, thickness, h);
    rect_fill(x + w - thickness, y, thickness, h);
}

static void filled_circle_quadrant(float cx, float cy, float r, int quadrant) {
    int rr = (int)std::ceil(r);
    for (int dy = 0; dy <= rr; ++dy) {
        float dx = std::sqrt(std::max(0.0f, r * r - (float)dy * dy));
        float x0, y0, w;
        switch (quadrant) {
            case 0: x0 = cx - dx; y0 = cy - dy; w = dx; break; // top-left
            case 1: x0 = cx;      y0 = cy - dy; w = dx; break; // top-right
            case 2: x0 = cx - dx; y0 = cy + dy; w = dx; break; // bottom-left
            case 3: x0 = cx;      y0 = cy + dy; w = dx; break; // bottom-right
            default: return;
        }
        SDL_FRect rct{x0 + g_tx, y0 + g_ty, w, 1.0f};
        SDL_RenderFillRect(g_app.renderer, &rct);
    }
}

void rect_fill_rounded(float x, float y, float w, float h, float radius) {
    if (radius <= 0.5f) { rect_fill(x, y, w, h); return; }
    float r = std::min({radius, w * 0.5f, h * 0.5f});
    rect_fill(x + r, y, w - 2 * r, h);
    rect_fill(x, y + r, r, h - 2 * r);
    rect_fill(x + w - r, y + r, r, h - 2 * r);
    filled_circle_quadrant(x + r, y + r, r, 0);
    filled_circle_quadrant(x + w - r, y + r, r, 1);
    filled_circle_quadrant(x + r, y + h - r, r, 2);
    filled_circle_quadrant(x + w - r, y + h - r, r, 3);
}

void rect_line_rounded(float x, float y, float w, float h, float radius, float thickness) {
    // Approximate: draw four straight lines ignoring corner curve. Good enough.
    rect_line(x + radius, y, w - 2 * radius, thickness);
    rect_line(x + radius, y + h - thickness, w - 2 * radius, thickness);
    rect_line(x, y + radius, thickness, h - 2 * radius);
    rect_line(x + w - thickness, y + radius, thickness, h - 2 * radius);
}

void circle_fill(float cx, float cy, float r) {
    int rr = (int)std::ceil(r);
    for (int dy = -rr; dy <= rr; ++dy) {
        float dx = std::sqrt(std::max(0.0f, r * r - (float)dy * dy));
        SDL_FRect rct{cx - dx + g_tx, cy + dy + g_ty, 2 * dx, 1.0f};
        SDL_RenderFillRect(g_app.renderer, &rct);
    }
}

void line(float x1, float y1, float x2, float y2, float thickness) {
    (void)thickness;
    SDL_RenderLine(g_app.renderer, x1 + g_tx, y1 + g_ty, x2 + g_tx, y2 + g_ty);
}

void polyline(const std::vector<float>& xy) {
    if (xy.size() < 4) return;
    for (size_t i = 0; i + 3 < xy.size(); i += 2) {
        SDL_RenderLine(g_app.renderer,
            xy[i] + g_tx, xy[i + 1] + g_ty,
            xy[i + 2] + g_tx, xy[i + 3] + g_ty);
    }
}

void render_geometry(const std::vector<SDL_Vertex>& verts, const std::vector<int>& indices, SDL_Texture* tex) {
    if (verts.empty()) return;
    std::vector<SDL_Vertex> tv = verts;
    for (auto& v : tv) {
        v.position.x += g_tx;
        v.position.y += g_ty;
    }
    if (indices.empty()) {
        SDL_RenderGeometry(g_app.renderer, tex, tv.data(), (int)tv.size(), nullptr, 0);
    } else {
        SDL_RenderGeometry(g_app.renderer, tex, tv.data(), (int)tv.size(), indices.data(), (int)indices.size());
    }
}

void push_translate(float dx, float dy) {
    g_xform_stack.push_back({g_tx, g_ty});
    g_tx += dx;
    g_ty += dy;
}

void pop_translate() {
    if (g_xform_stack.empty()) { g_tx = 0; g_ty = 0; return; }
    auto p = g_xform_stack.back();
    g_xform_stack.pop_back();
    g_tx = p.first;
    g_ty = p.second;
}

void get_translate(float& tx, float& ty) { tx = g_tx; ty = g_ty; }

void set_clip(int x, int y, int w, int h) {
    SDL_Rect r{x, y, w, h};
    SDL_SetRenderClipRect(g_app.renderer, &r);
}
void clear_clip() {
    SDL_SetRenderClipRect(g_app.renderer, nullptr);
}

void draw_texture(SDL_Texture* tex, float dst_x, float dst_y, float dst_w, float dst_h) {
    if (!tex) return;
    SDL_FRect dst{dst_x + g_tx, dst_y + g_ty, dst_w, dst_h};
    SDL_RenderTexture(g_app.renderer, tex, nullptr, &dst);
}

void draw_texture_rotated(SDL_Texture* tex, float cx, float cy, float w, float h, float angle_rad) {
    if (!tex) return;
    SDL_FRect dst{cx - w * 0.5f + g_tx, cy - h * 0.5f + g_ty, w, h};
    SDL_FPoint center{w * 0.5f, h * 0.5f};
    SDL_RenderTextureRotated(g_app.renderer, tex, nullptr, &dst, angle_rad * 180.0 / M_PI, &center, SDL_FLIP_NONE);
}

void draw_texture_scaled(SDL_Texture* tex, float anchor_x, float anchor_y, float scale, float origin_u, float origin_v) {
    if (!tex) return;
    float w, h;
    SDL_GetTextureSize(tex, &w, &h);
    float dw = w * scale, dh = h * scale;
    SDL_FRect dst{anchor_x - dw * origin_u + g_tx, anchor_y - dh * origin_v + g_ty, dw, dh};
    SDL_RenderTexture(g_app.renderer, tex, nullptr, &dst);
}

} // namespace render
