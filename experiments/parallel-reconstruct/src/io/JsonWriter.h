#pragma once

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "datastructures.h"

using json = nlohmann::json;

class JsonWriter {
 public:
  void write(Points& models, const std::filesystem::path& path);
};
