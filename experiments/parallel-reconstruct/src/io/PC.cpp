#include "PC.h"

#include <spdlog/sinks/basic_file_sink.h>

Points read_pointcloud(uint nr_points) {
  auto logger = spdlog::get("read_pc");
  uint default_nr_points = 100000;
  if (nr_points == 0) {
    spdlog::info("Defaulting to {} point cloud points", default_nr_points);
    nr_points = default_nr_points;
  };
  Points pointcloud;
  for (auto i = 0; i < nr_points; i++) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(3));
    pointcloud.x.push_back(42.0);
    pointcloud.y.push_back(42.0);
    pointcloud.z.push_back(42.0);
    if (i % 1000 == 0) {
      logger->trace(i);
    }
  }
  return pointcloud;
}
