#pragma once
#include <string>
#include <vector>

struct Category {
    std::string id;
    std::string name;
    std::string icon; // path relative to assets dir
    std::string path; // empty = no path
    std::string filter; // "music"|"photo"|"video"|"" (none)
};

namespace categories {
std::vector<Category>& list();
} // namespace categories
