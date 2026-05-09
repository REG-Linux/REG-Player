#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <unordered_map>

namespace assets {

struct Fonts {
    TTF_Font* main = nullptr;
    TTF_Font* small = nullptr;
    TTF_Font* title = nullptr;
    TTF_Font* artist = nullptr;
    TTF_Font* album = nullptr;
    TTF_Font* time_elapsed = nullptr;
    TTF_Font* time_dur = nullptr;
    TTF_Font* xs = nullptr;
    TTF_Font* large = nullptr;
};

extern std::unordered_map<std::string, SDL_Texture*> images;
extern Fonts fonts;

void load();
void unload();

SDL_Texture* get_image(const std::string& name);
void play_sfx(const std::string& name);

std::string assets_dir();

} // namespace assets
