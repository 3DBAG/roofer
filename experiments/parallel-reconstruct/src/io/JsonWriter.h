#pragma once

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class JsonWriter {
 public:
  void write(json& data, const std::filesystem::path& path);
};
