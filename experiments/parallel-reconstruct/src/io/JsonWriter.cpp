#include "JsonWriter.h"
void roofer::io::JsonWriter::write(json& data,
                                   const std::filesystem::path& path) {
  std::ofstream o(path);
  o << data << std::endl;
}
