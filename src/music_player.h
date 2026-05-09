#pragma once
#include "ui.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace music_player {

struct Track { std::string name; std::string path; };

struct Tags {
    std::string title;
    std::string artist;
    std::string album;
};

struct State {
    bool active = false;
    bool playing = false;
    bool paused = false;
    Track current_track;
    int current_index = 0;
    std::vector<Track> playlist;
    double elapsed = 0;
    double duration = 0;
    void* cover_art = nullptr; // SDL_Texture*
    bool repeat_one = false;
    int auto_sleep_minutes = 0;
    double auto_sleep_remaining = -1;
    std::string visualizer_mode = "wave"; // off|wave|bars
    float fade_alpha = 0;
    std::unordered_map<std::string, ui::Marquee> marquees;
    Tags tags;
    Tags next_tags;
};

extern State s;

void init();
void play(const std::string& filepath, const std::vector<Track>* custom_playlist = nullptr);
void stop();
void toggle_pause();
void next_track();
void prev_track();
void close();
void set_repeat_one(bool e);
void set_auto_sleep_minutes(int m);
void set_visualizer_mode(const std::string& m);
void update(float dt);

const float* pcm();
int pcm_count();
int pcm_pos();

} // namespace music_player
