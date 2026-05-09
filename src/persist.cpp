#include "persist.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace persist {

bool read_file(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool write_file(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), data.size());
    return f.good();
}

bool read_json(const std::string& path, nlohmann::json& out) {
    std::string s;
    if (!read_file(path, s)) return false;
    try {
        out = nlohmann::json::parse(s);
    } catch (...) {
        return false;
    }
    return true;
}

bool write_json(const std::string& path, const nlohmann::json& j) {
    return write_file(path, j.dump(2));
}

bool file_exists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

} // namespace persist
