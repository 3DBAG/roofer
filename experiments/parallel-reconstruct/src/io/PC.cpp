#include "PC.h"

#include <spdlog/sinks/basic_file_sink.h>

const auto SLEEP_AT_OPEN = std::chrono::seconds(2);
const auto SLEEP_PER_POINT = std::chrono::nanoseconds(1);
const uint EMIT_TRACE_AT = 1000;

ReturnPoints read_pointcloud_coro(uint nr_laz, uint nr_points_per_laz) {
  auto logger = spdlog::get("read_pc");
  uint default_nr_points = 100000;
  if (nr_points_per_laz == 0) {
    spdlog::info("Defaulting to {} point cloud points", default_nr_points);
    nr_points_per_laz = default_nr_points;
  };

  std::this_thread::sleep_for(SLEEP_AT_OPEN);

  Points pointcloud;
  for (auto i = 0; i < nr_points_per_laz; i++) {
    // Imitate slow I/O
    std::this_thread::sleep_for(SLEEP_PER_POINT);
    auto offset = nr_laz * nr_points_per_laz;
    auto _p = static_cast<float>(i + offset);
    pointcloud.x.push_back(_p);
    pointcloud.y.push_back(_p);
    pointcloud.z.push_back(_p);
    if (i % EMIT_TRACE_AT == 0) {
      logger->trace(i);
    }
  }
  logger->trace(nr_points_per_laz);
  co_return pointcloud;
}

Points read_pointcloud(uint nr_laz, uint nr_points_per_laz) {
  auto logger = spdlog::get("read_pc");
  uint default_nr_points = 100000;
  if (nr_points_per_laz == 0) {
    spdlog::info("Defaulting to {} point cloud points", default_nr_points);
    nr_points_per_laz = default_nr_points;
  };

  std::this_thread::sleep_for(SLEEP_AT_OPEN);

  Points pointcloud;
  for (auto i = 0; i < nr_points_per_laz; i++) {
    // Imitate slow I/O
    std::this_thread::sleep_for(SLEEP_PER_POINT);
    auto offset = nr_laz * nr_points_per_laz;
    auto _p = static_cast<float>(i + offset);
    pointcloud.x.push_back(_p);
    pointcloud.y.push_back(_p);
    pointcloud.z.push_back(_p);
    if (i % EMIT_TRACE_AT == 0) {
      logger->trace(i);
    }
  }
  logger->trace(nr_points_per_laz);
  return pointcloud;
}
