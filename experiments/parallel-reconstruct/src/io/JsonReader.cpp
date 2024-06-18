#include "JsonReader.h"
json roofer::io::JsonReader::read(const std::filesystem::path& path) {
  std::ifstream f(path);
  json data = json::parse(f);
  f.close();
  return data;
}
