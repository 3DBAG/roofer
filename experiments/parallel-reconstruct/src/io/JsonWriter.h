#pragma once

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace roofer::io {
  class JsonWriter {
    void write(json& data, const std::filesystem::path& path);
  };
}  // namespace roofer::io
