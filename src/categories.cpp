#include "categories.h"

namespace categories {

std::vector<Category>& list() {
    static std::vector<Category> cats = {
        {"settings", "Settings", "icons/settings.png", "",                          ""},
        {"photo",    "Photo",    "icons/photo.png",    "/userdata/medias/photos",   "photo"},
        {"music",    "Music",    "icons/music.png",    "/userdata/medias/music",    "music"},
        {"video",    "Video",    "icons/video.png",    "/userdata/medias/videos",   "video"},
        {"folder",   "Files",    "icons/folders.png",  "/userdata/medias",          ""},
    };
    return cats;
}

} // namespace categories
