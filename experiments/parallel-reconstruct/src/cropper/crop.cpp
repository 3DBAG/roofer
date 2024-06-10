#include "crop.h"

#include <spdlog/sinks/basic_file_sink.h>

#include <chrono>
#include <coroutine>
#include <cstdint>
#include <thread>

#include "spdlog/spdlog.h"

void crop_coro(uint nr_points_per_laz, uint nr_laz,
               std::queue<Points>& queue_cropped) {
  auto logger = spdlog::get("crop");
  std::vector<Points> pointcloud_per_building;
  uint total_building_count = 0;
  for (auto laz = 0; laz < nr_laz; laz++) {
    logger->debug("Starting with laz {}", laz);
    auto return_points = read_pointcloud_coro(nr_points_per_laz);
    auto pointcloud = return_points.mCoroHdl.promise()._valueOut;

    uint nr_pts_per_building = 100;
    uint part_count = 0;
    Points pts;
    for (float i : pointcloud.x) {
      // Here is the point-in-polygon test
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      // After the pip test with push the point to the polygon
      pts.x.push_back(i);
      pts.y.push_back(i);
      pts.z.push_back(i);
      // Add a the cropped building points to the collection
      part_count++;
      if (part_count == nr_pts_per_building) {
        part_count = 0;
        queue_cropped.push(pts);
        if (total_building_count % 100 == 0) {
          logger->trace(total_building_count);
        }
        total_building_count++;
        pts = Points();
      }
    }
    // Yield the collection of Points from the laz, suspend this coroutine and
    // pass on the execution to reconstruct.
    // But the crop needs to continue...
    logger->trace(total_building_count);
    logger->debug("Finished with laz {}", laz);
  }
}

void crop(uint nr_points_per_laz, uint nr_laz,
          std::queue<Points>& queue_cropped) {
  auto logger = spdlog::get("crop");
  std::vector<Points> pointcloud_per_building;
  uint total_building_count = 0;
  for (auto laz = 0; laz < nr_laz; laz++) {
    auto pointcloud = read_pointcloud(nr_points_per_laz);
    // I/O time
    std::this_thread::sleep_for(std::chrono::seconds(5));

    uint nr_pts_per_building = 100;
    uint part_count = 0;
    Points pts;
    for (float i : pointcloud.x) {
      // Here is the point-in-polygon test
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      // After the pip test with push the point to the polygon
      pts.x.push_back(i);
      pts.y.push_back(i);
      pts.z.push_back(i);
      // Add a the cropped building points to the collection
      part_count++;
      if (part_count == nr_pts_per_building) {
        part_count = 0;
        queue_cropped.push(pts);
        if (total_building_count % 100 == 0) {
          logger->trace(total_building_count);
        }
        total_building_count++;
        pts = Points();
      }
    }
    logger->trace(total_building_count);
  }
}
