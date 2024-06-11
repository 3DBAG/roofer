#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <coroutine>
#include <format>
#include <nlohmann/json.hpp>
#include <queue>
#include <stdexcept>

#include "JsonWriter.h"
#include "crop.h"
#include "reconstruct.h"

using json = nlohmann::json;

int main() {
  std::string jsonpattern = {
      "{\"time\": \"%Y-%m-%dT%H:%M:%S.%f%z\", \"name\": \"%n\", \"process\": "
      "%P, "
      "\"thread\": %t, "
      "\"count\": %v}"};
  spdlog::set_pattern(jsonpattern);
  spdlog::set_level(spdlog::level::trace);

  // Init loggers
  auto logs_dir = "logs/async_coro";
  auto logger_read_pc = spdlog::basic_logger_mt(
      "read_pc", std::format("{}/log_read_pc.json", logs_dir));
  auto logger_crop = spdlog::basic_logger_mt(
      "crop", std::format("{}/log_crop.json", logs_dir));
  auto logger_reconstruct = spdlog::basic_logger_mt(
      "reconstruct", std::format("{}/log_reconstruct.json", logs_dir));
  auto logger_write = spdlog::basic_logger_mt(
      "write", std::format("{}/log_write.json", logs_dir));
  auto logger_coro = spdlog::stderr_color_mt("coro");
  //  auto logger_coro = spdlog::basic_logger_mt(
  //    "coro", std::format("{}/log_coro.json", logs_dir));

  // Crop
  std::queue<Points> queue_cropped_buildings;
  uint nr_laz_files = 3;
  uint nr_points_per_laz = 2000;
  std::vector<Points> cropped_points{};
  auto cropped_generator = crop_coro(nr_points_per_laz, nr_laz_files);
  uint counter = 0;
  while (!cropped_generator.mCoroHdl.done()) {
    if (counter > nr_laz_files * nr_points_per_laz) {
      logger_coro->critical(
          "cropped_generator loop passed the expected amount of points");
      throw std::runtime_error(
          "cropped_generator loop passed the expected amount of points");
    }
    auto pts = cropped_generator.mCoroHdl.promise()._valueOut;
    cropped_points.emplace_back(pts);
    //    logger_coro->debug("cropped_points counter {}, first {} last {} size
    //    {}", counter, pts.x.front(), pts.x.back(), pts.x.size());
    counter++;
    cropped_generator.mCoroHdl.resume();
  }
  logger_coro->debug("cropped_points length {}", cropped_points.size());

  return 0;
}
