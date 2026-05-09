#include "browser.h"
#include "indexing.h"
#include "utils.h"
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

namespace browser {

std::string base_dir = "/userdata/medias";
std::string current_dir = base_dir;
std::vector<File> files;
std::string filter;

void set_filter(const std::string& type) { filter = type; }
void set_state(const std::string& b, const std::string& c, const std::string& f) {
    base_dir = b; current_dir = c; filter = f;
}
void set_files(std::vector<File> list) { files = std::move(list); }

static bool ext_in_filter(const std::string& ext, const std::string& f) {
    if (f.empty()) return true;
    const auto& ex = indexing::compatible_extensions(f);
    for (auto& e : ex) if (e == ext) return true;
    return false;
}

void scan(const std::string& path_) {
    std::string path = path_.empty() ? current_dir : path_;
    files.clear();
    DIR* d = opendir(path.c_str());
    if (!d) return;
    std::vector<File> dirs, fs;
    while (auto* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        std::string full = path + "/" + e->d_name;
        struct stat st;
        // lstat to detect symlinks — skip them to avoid loops.
        if (lstat(full.c_str(), &st) != 0) continue;
        if (S_ISLNK(st.st_mode)) continue;
        if (S_ISDIR(st.st_mode)) {
            dirs.push_back({e->d_name, full, "directory", "", "", "", "", -1, -1, "", {}, nullptr});
        } else if (S_ISREG(st.st_mode)) {
            std::string ext = utils::get_extension(full);
            if (ext_in_filter(ext, filter)) {
                fs.push_back({e->d_name, full, "file", "", "", "", "", -1, -1, "", {}, nullptr});
            }
        }
    }
    closedir(d);
    auto cmp = [](const File& a, const File& b) {
        std::string la = a.name, lb = b.name;
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        return la < lb;
    };
    std::sort(dirs.begin(), dirs.end(), cmp);
    std::sort(fs.begin(), fs.end(), cmp);
    for (auto& v : dirs) files.push_back(std::move(v));
    for (auto& v : fs)   files.push_back(std::move(v));
    if (files.empty()) {
        File f; f.name = "Empty Folder"; f.type = "info";
        files.push_back(f);
    }
}

} // namespace browser
