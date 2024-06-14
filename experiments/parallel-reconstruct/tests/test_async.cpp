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
#include "reconstruct.h"

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
          auto reconstructed_one_laz =
              reconstruct_batch(deque_cropped_laz.front());
          auto models_one_laz =
              reconstructed_one_laz.mCoroHdl.promise()._valueOut;
          deque_reconstructed_buildings.push_back(models_one_laz);
          for (auto i = 0; i < models_one_laz.size(); i++) {
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
