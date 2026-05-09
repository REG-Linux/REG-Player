#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace persist {

bool read_json(const std::string& path, nlohmann::json& out);
bool write_json(const std::string& path, const nlohmann::json& j);

bool read_file(const std::string& path, std::string& out);
bool write_file(const std::string& path, const std::string& data);

bool file_exists(const std::string& path);

} // namespace persist
