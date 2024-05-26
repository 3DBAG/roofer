#include "JsonWriter.h"
void JsonWriter::write(json& data, const std::filesystem::path& path) {
  std::ofstream o(path);
  o << data << '\n';
}
