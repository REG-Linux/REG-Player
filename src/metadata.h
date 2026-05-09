#pragma once
#include <string>
#include <vector>

namespace metadata {

struct Tags {
    std::string title;
    std::string artist;
    std::string album;
    std::string album_artist;
    int track_number = -1;
    int disc_number = -1;
    std::vector<unsigned char> cover_data;
    std::string cover_ext;
};

Tags get_tags(const std::string& filepath);
bool find_folder_cover(const std::string& track_path, std::vector<unsigned char>& out, std::string& out_ext);

} // namespace metadata
