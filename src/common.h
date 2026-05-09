#pragma once

#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct Color {
    float r{1}, g{1}, b{1}, a{1};
    Color() = default;
    Color(float R, float G, float B) : r(R), g(G), b(B), a(1.0f) {}
    Color(float R, float G, float B, float A) : r(R), g(G), b(B), a(A) {}
};

struct AppContext {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    int screen_w{};
    int screen_h{};
    bool quit_requested{false};
    bool restart_requested{false};
};

extern AppContext g_app;
