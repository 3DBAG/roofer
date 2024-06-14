#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <atomic>
#include <coroutine>
#include <deque>
#include <format>
#include <span>
#include <stdexcept>
#include <thread>

#include "JsonWriter.h"
#include "crop.h"
#include "libfork/core.hpp"
#include "libfork/schedule.hpp"
#include "reconstruct.h"

const uint EMIT_TRACE_AT = 10;

// Ref:
// https://github.com/ConorWilliams/libfork?tab=readme-ov-file#the-cactus-stack
inline constexpr auto reconstruct_parallel =
    [](auto reconstruct_parallel,
       std::span<Points> cropped_buildings) -> lf::task<std::vector<Points>> {
  // Allocate space for results, outputs is a std::span<Points>
  auto [outputs] = co_await lf::co_new<Points>(cropped_buildings.size());

  for (std::size_t i = 0; i < cropped_buildings.size(); ++i) {
    co_await lf::fork[&outputs[i], reconstruct_one_building_libfork](
        cropped_buildings[i]);
  }

  co_await lf::join;  // Wait for all tasks to complete.

  std::vector<Points> o{};
  o.assign(outputs.begin(), outputs.end());

  co_return o;
};

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
  auto logs_dir = "logs/async";
  auto logger_read_pc = spdlog::basic_logger_mt(
      "read_pc", std::format("{}/log_read_pc.json", logs_dir));
  auto logger_crop = spdlog::basic_logger_mt(
      "crop", std::format("{}/log_crop.json", logs_dir));
  auto logger_reconstruct = spdlog::basic_logger_mt(
      "reconstruct", std::format("{}/log_reconstruct.json", logs_dir));
  auto logger_write = spdlog::basic_logger_mt(
      "write", std::format("{}/log_write.json", logs_dir));
  auto logger_coro = spdlog::stderr_color_mt("coro");

  // This should be something like:
  // unsigned int nthreads = std::thread::hardware_concurrency();
  // lf::lazy_pool pool(nthreads - 3);
  // -3, because we need one thread for loops of crop, reconstruct, serialize
  lf::lazy_pool pool(4);  // 4 worker threads

  // Yields the cropped points per laz file.
  std::deque<std::vector<Points>> deque_cropped_laz;
  auto cropped_generator = crop_coro_batch(nr_points_per_laz, nr_laz_files);
  std::thread cropper([&]() {
    while (!cropped_generator.mCoroHdl.done()) {
      auto cropped_per_laz = cropped_generator.mCoroHdl.promise()._valueOut;
      deque_cropped_laz.push_back(cropped_per_laz);
      cropped_generator.mCoroHdl.resume();
    }
  });
  // When crop yields the data of one laz, it suspends, add the data to the
  // queue When crop is finished, it needs to signal somehow that it's finished
  // Reconstruct_batch is initially suspended
  // There needs to be a container/buffer between crop and reconstruct so that
  // the two can operate at different speeds.
  // When data is available in the queue, resume reconstruct_batch
  // When reconstruct_batch finished, it suspends

  // Reconstruct
  std::deque<std::vector<Points>> deque_reconstructed_buildings;
  std::atomic<bool> reconstruction_running{true};
  std::atomic<uint> count_buildings{0};
  logger_reconstruct->trace(count_buildings.load());
  std::thread reconstructor([&]() {
    while (!cropped_generator.mCoroHdl.done()) {
      if (!deque_cropped_laz.empty()) {
        for (; !deque_cropped_laz.empty(); deque_cropped_laz.pop_front()) {
          logger_coro->debug("popped one item from deque_cropped_laz");
          auto reconstructed_one_laz = lf::sync_wait(pool, reconstruct_parallel,
                                                     deque_cropped_laz.front());
          deque_reconstructed_buildings.push_back(reconstructed_one_laz);
          for (auto i = 0; i < reconstructed_one_laz.size(); i++) {
            count_buildings++;
            if (count_buildings % EMIT_TRACE_AT == 0) {
              logger_reconstruct->trace(count_buildings.load());
            }
          }
        }
      }
    }
    reconstruction_running.store(false);
  });

  // Serialize
  std::atomic<uint> count_written{0};
  auto json_writer = JsonWriter();
  logger_write->trace(count_written.load());
  std::thread serializer([&]() {
    while (reconstruction_running.load()) {
      if (!deque_reconstructed_buildings.empty()) {
        for (; !deque_reconstructed_buildings.empty();
             deque_reconstructed_buildings.pop_front()) {
          auto models_one_laz = deque_reconstructed_buildings.front();
          for (auto model : models_one_laz) {
            json_writer.write(model, std::format("output/async/{}.json",
                                                 count_written.load()));
            count_written++;
            if (count_written % EMIT_TRACE_AT == 0) {
              logger_write->trace(count_written.load());
            }
          }
        }
      }
    }
  });

  cropper.join();
  reconstructor.join();
  serializer.join();

  logger_reconstruct->trace(count_buildings.load());
  logger_write->trace(count_written.load());

  return 0;
}
