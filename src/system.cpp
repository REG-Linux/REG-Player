// Generic Linux replacements for muOS sysfs reads.
#include "system.h"
#include "persist.h"
#include <alsa/asoundlib.h>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

namespace system_info {

static int parse_int(const std::string& s) {
    return std::atoi(s.c_str());
}

static std::vector<std::string> glob_dir(const char* dir, const char* prefix) {
    std::vector<std::string> out;
    DIR* d = opendir(dir);
    if (!d) return out;
    while (auto* e = readdir(d)) {
        if (std::strncmp(e->d_name, prefix, std::strlen(prefix)) == 0) {
            out.push_back(std::string(dir) + "/" + e->d_name);
        }
    }
    closedir(d);
    return out;
}

std::optional<int> get_battery_percentage() {
    auto bats = glob_dir("/sys/class/power_supply", "BAT");
    for (auto& p : bats) {
        std::string s;
        if (persist::read_file(p + "/capacity", s)) {
            return parse_int(s);
        }
    }
    return std::nullopt;
}

bool is_charging() {
    auto bats = glob_dir("/sys/class/power_supply", "BAT");
    for (auto& p : bats) {
        std::string s;
        if (persist::read_file(p + "/status", s)) {
            // trim
            while (!s.empty() && (s.back() == '\n' || s.back() == ' ')) s.pop_back();
            if (s == "Charging" || s == "Full") return true;
        }
    }
    // Also check AC adapter online flag
    auto acs = glob_dir("/sys/class/power_supply", "AC");
    for (auto& p : acs) {
        std::string s;
        if (persist::read_file(p + "/online", s)) {
            if (parse_int(s) == 1) return true;
        }
    }
    return false;
}

std::optional<int> get_volume() {
    snd_mixer_t* mixer = nullptr;
    if (snd_mixer_open(&mixer, 0) < 0) return std::nullopt;
    if (snd_mixer_attach(mixer, "default") < 0) { snd_mixer_close(mixer); return std::nullopt; }
    if (snd_mixer_selem_register(mixer, nullptr, nullptr) < 0) { snd_mixer_close(mixer); return std::nullopt; }
    if (snd_mixer_load(mixer) < 0) { snd_mixer_close(mixer); return std::nullopt; }

    snd_mixer_selem_id_t* sid;
    snd_mixer_selem_id_alloca(&sid);
    long min = 0, max = 0, vol = 0;

    const char* candidates[] = {"Master", "PCM", "Speaker", nullptr};
    for (int i = 0; candidates[i]; ++i) {
        snd_mixer_selem_id_set_index(sid, 0);
        snd_mixer_selem_id_set_name(sid, candidates[i]);
        snd_mixer_elem_t* elem = snd_mixer_find_selem(mixer, sid);
        if (!elem) continue;
        if (snd_mixer_selem_get_playback_volume_range(elem, &min, &max) < 0) continue;
        if (snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &vol) < 0) continue;
        snd_mixer_close(mixer);
        if (max <= min) return 0;
        return (int)((vol - min) * 100 / (max - min));
    }
    snd_mixer_close(mixer);
    return std::nullopt;
}

std::optional<int> get_brightness() {
    DIR* d = opendir("/sys/class/backlight");
    if (!d) return std::nullopt;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        std::string base = std::string("/sys/class/backlight/") + e->d_name;
        std::string cur, max;
        if (persist::read_file(base + "/brightness", cur) &&
            persist::read_file(base + "/max_brightness", max)) {
            int c = parse_int(cur), m = parse_int(max);
            closedir(d);
            if (m <= 0) return 0;
            return (c * 100) / m;
        }
    }
    closedir(d);
    return std::nullopt;
}

} // namespace system_info
