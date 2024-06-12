#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <coroutine>
#include <format>
#include <future>
#include <nlohmann/json.hpp>
#include <queue>
#include <stdexcept>

#include "JsonWriter.h"
#include "crop.h"
#include "reconstruct.h"

using json = nlohmann::json;

const uint EMIT_TRACE_AT = 10;

int main(int argc, char* argv[]) {
  auto args = std::span(argv, size_t(argc));
  uint nr_laz_files = 3;
  nr_laz_files = std::stoi(args[1]);
  uint nr_points_per_laz = 2000;
  nr_points_per_laz = std::stoi(args[2]);

  std::string jsonpattern = {
      "{\"time\": \"%Y-%m-%dT%H:%M:%S.%f%z\", \"name\": \"%n\", \"process\": "
      "%P, "
      "\"thread\": %t, "
      "\"count\": %v}"};
  spdlog::set_pattern(jsonpattern);
  spdlog::set_level(spdlog::level::trace);

  // Init loggers
  auto logs_dir = "logs/parallel_reconstruct";
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
  crop(nr_points_per_laz, nr_laz_files, queue_cropped_buildings);

  // Reconstruct
  std::queue<Points> queue_reconstructed_buildings;
  auto count_buildings = 0;
  logger_reconstruct->trace(count_buildings);

  auto json_writer = JsonWriter();

  logger_crop->trace(count_buildings);
  logger_reconstruct->trace(count_buildings);
  logger_write->trace(count_buildings);

  return 0;
}
