#pragma once
#include <string>
#include <vector>

namespace chiptune {

// All supported extensions (lowercase, with dot).
const std::vector<std::string>& extensions();

// True if path's extension is one we handle via libxmp or libgme.
// Accepts "file.nsf" or "file.nsf#5" (sub-tune index suffix).
bool is_chiptune(const std::string& path);

// Decode entire file. Returns sample rate.
// Fills `stereo` with interleaved L,R,L,R... float PCM (channels=2).
// Fills `mono` with per-frame float (averaged L+R) — used by visualizer.
// Caps duration to max_seconds for looping formats.
// Path may include "#N" suffix to select sub-tune N (libgme formats).
// Returns 0 on failure.
int decode_to_pcm(const std::string& path,
                  std::vector<float>& stereo,
                  std::vector<float>& mono,
                  int max_seconds = 300);

// Number of sub-tunes in a file. 1 for single-track formats.
int subtune_count(const std::string& path);

// Split "file.nsf#3" into "file.nsf" + 3. Returns 0 if no suffix.
void split_subtune(const std::string& full_path, std::string& base_path, int& track_idx);

struct Tags {
    std::string title;
    std::string artist;
    std::string album;   // mapped from "system" / "game"
    std::string copyright;
    std::string system;
    std::string dumper;
    std::string comment;
    std::string release_date;
    std::string game_jp;
    int length_ms = 0;        // total length (or play_length+fade)
    int intro_length_ms = 0;
    int loop_length_ms  = 0;
    bool has_value = false;
};
Tags read_tags(const std::string& path);

} // namespace chiptune
