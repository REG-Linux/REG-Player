#pragma once
#include "browser.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace image_viewer {

struct State {
    bool active = false;
    SDL_Texture* current_image = nullptr;
    std::string current_path;
    std::vector<browser::File> playlist;
    int current_index = 0;
    float zoom = 1.0f;
    float pan_x = 0;
    float pan_y = 0;
    float fade_alpha = 0;
};

extern State s;

void init();
void open(const std::string& path, const std::vector<browser::File>& files_list);
void load_image(const std::string& path);
void reset_view();
void next_image();
void prev_image();
void close();
void update(float dt);

} // namespace image_viewer
