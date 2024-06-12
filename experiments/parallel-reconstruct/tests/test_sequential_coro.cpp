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

// struct reconstruct_all_result {
//   struct promise_type {
//     void unhandled_exception() noexcept {}
//
//     reconstruct_all_result get_return_object() { return {}; }
//
//     std::suspend_never initial_suspend() noexcept { return {}; }
//
//     std::suspend_never final_suspend() noexcept { return {}; }
//   };
// };

// reconstruct_all_result reconstruct_all(uint nr_points_per_laz, uint
// nr_laz_files) {
//   auto cropped_generator = crop_coro(nr_points_per_laz, nr_laz_files);
//   while (auto cropped_buildings_one_laz = co_await
//   crop_coro(nr_points_per_laz, nr_laz_files)) {
//     auto models = lf::fork[reconstruct_one](cropped_buildings_one_laz);
//     serialize(models);
//   }
// }

int main() {
  std::string jsonpattern = {
      "{\"time\": \"%Y-%m-%dT%H:%M:%S.%f%z\", \"name\": \"%n\", \"process\": "
      "%P, "
      "\"thread\": %t, "
      "\"count\": %v}"};
  spdlog::set_pattern(jsonpattern);
  spdlog::set_level(spdlog::level::trace);

  // Init loggers
  auto logs_dir = "logs/sequential_coro";
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
  auto cropped_generator = crop_coro(nr_points_per_laz, nr_laz_files);
  uint count_buildings = 0;
  auto json_writer = JsonWriter();
  while (!cropped_generator.mCoroHdl.done()) {
    if (count_buildings > nr_laz_files * nr_points_per_laz) {
      logger_coro->critical(
          "cropped_generator loop passed the expected amount of points");
      throw std::runtime_error(
          "cropped_generator loop passed the expected amount of points");
    }
    auto pts = cropped_generator.mCoroHdl.promise()._valueOut;

    auto model = reconstruct_one_building(pts);
    if (count_buildings % 10 == 0) {
      logger_reconstruct->trace(count_buildings);
    }

    json_writer.write(
        model, std::format("output/sequential_coro/{}.json", count_buildings));
    if (count_buildings % 10 == 0) {
      logger_write->trace(count_buildings);
    }

    count_buildings++;
    cropped_generator.mCoroHdl.resume();
  }
  logger_crop->trace(count_buildings);
  logger_reconstruct->trace(count_buildings);
  logger_write->trace(count_buildings);

  return 0;
}
