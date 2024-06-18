#include "crop.h"

#include <spdlog/sinks/basic_file_sink.h>

#include <chrono>
#include <thread>

#include "spdlog/spdlog.h"

const uint NR_PTS_PER_BUILDING = 100;
const uint EMIT_TRACE_AT = 10;
const auto SLEEP_PER_POINT = std::chrono::milliseconds(1);

GenerateCroppedPointsBatch crop_coro_batch(uint nr_points_per_laz,
                                           uint nr_laz) {
  auto logger_crop = spdlog::get("crop");
  auto logger_coro = spdlog::get("coro");
  uint total_building_count = 0;

  // The function receives all the available laz files as input, and processes
  // them one after the other.
  for (auto laz = 0; laz < nr_laz; laz++) {
    logger_coro->debug("Starting with laz {}", laz);

    std::vector<Points> cropped_per_laz;

    // Wait that the file is opened and ready
    auto return_points = read_pointcloud_coro(laz, nr_points_per_laz);
    auto pointcloud = return_points.mCoroHdl.promise()._valueOut;

    // Here is the point cloud processing logic. For this coroutine example it
    // is not important what happens in this block, as long as the cropped
    // points per building are pushed into a container (cropped_per_laz) per
    // laz file.

    uint part_count = 0;
    Points pts{};
    for (float i : pointcloud.x) {
      // Here is the point-in-polygon test
      std::this_thread::sleep_for(SLEEP_PER_POINT);
      // After the pip test with push the point to the polygon
      pts.x.push_back(i);
      pts.y.push_back(i);
      pts.z.push_back(i);
      // Add a the cropped building points to the collection
      part_count++;
      if (part_count == NR_PTS_PER_BUILDING) {
        part_count = 0;
        if (total_building_count % EMIT_TRACE_AT == 0) {
          logger_crop->trace(total_building_count);
        }
        total_building_count++;
        cropped_per_laz.push_back(pts);
        pts = Points();
      }
    }

    // Yield the cropped points of the whole laz file.
    co_yield cropped_per_laz;
    logger_crop->trace(total_building_count);
    logger_coro->debug("Finished with laz {}", laz);
  }
}

GenerateCroppedPoints crop_coro(uint nr_points_per_laz, uint nr_laz) {
  auto logger_crop = spdlog::get("crop");
  auto logger_coro = spdlog::get("coro");
  std::vector<Points> pointcloud_per_building;
  uint total_building_count = 0;
  for (auto laz = 0; laz < nr_laz; laz++) {
    logger_coro->debug("Starting with laz {}", laz);
    auto return_points = read_pointcloud_coro(laz, nr_points_per_laz);
    auto pointcloud = return_points.mCoroHdl.promise()._valueOut;
    //    logger_coro->debug("read_pointcloud_coro first {} last {} size {}",
    //    pointcloud.x.front(), pointcloud.x.back(), pointcloud.x.size());

    uint part_count = 0;
    Points pts{};
    for (float i : pointcloud.x) {
      // Here is the point-in-polygon test
      std::this_thread::sleep_for(SLEEP_PER_POINT);
      // After the pip test with push the point to the polygon
      pts.x.push_back(i);
      pts.y.push_back(i);
      pts.z.push_back(i);
      // Add a the cropped building points to the collection
      part_count++;
      if (part_count == NR_PTS_PER_BUILDING) {
        part_count = 0;
        if (total_building_count % EMIT_TRACE_AT == 0) {
          logger_crop->trace(total_building_count);
        }
        total_building_count++;
        // Yield the cropped points of a single building, suspend this coroutine
        // and pass on the execution to reconstruct. But the crop needs to
        // continue...
        co_yield pts;
        pts = Points();
      }
    }
    //    logger_crop->trace(total_building_count);
    logger_coro->debug("Finished with laz {}", laz);
  }
}

void crop(uint nr_points_per_laz, uint nr_laz,
          std::queue<Points>& queue_cropped) {
  auto logger_crop = spdlog::get("crop");
  std::vector<Points> pointcloud_per_building;
  uint total_building_count = 0;
  for (auto laz = 0; laz < nr_laz; laz++) {
    auto pointcloud = read_pointcloud(laz, nr_points_per_laz);

    uint part_count = 0;
    Points pts;
    for (float i : pointcloud.x) {
      // Here is the point-in-polygon test
      std::this_thread::sleep_for(SLEEP_PER_POINT);
      // After the pip test with push the point to the polygon
      pts.x.push_back(i);
      pts.y.push_back(i);
      pts.z.push_back(i);
      // Add a the cropped building points to the collection
      part_count++;
      if (part_count == NR_PTS_PER_BUILDING) {
        part_count = 0;
        queue_cropped.push(pts);
        if (total_building_count % EMIT_TRACE_AT == 0) {
          logger_crop->trace(total_building_count);
        }
        total_building_count++;
        pts = Points();
      }
    }
    logger_crop->trace(total_building_count);
  }
}

void crop(uint nr_points_per_laz, uint nr_laz,
          lf::deque<DequePoints>& queue_cropped) {
  auto logger_crop = spdlog::get("crop");
  std::vector<Points> pointcloud_per_building;
  uint total_building_count = 0;
  for (auto laz = 0; laz < nr_laz; laz++) {
    auto pointcloud = read_pointcloud(laz, nr_points_per_laz);

    uint part_count = 0;
    DequePoints pts{};
    for (float i : pointcloud.x) {
      // Here is the point-in-polygon test
      std::this_thread::sleep_for(SLEEP_PER_POINT);
      // After the pip test with push the point to the polygon
      //      pts.x.push_back(i);
      //      pts.y.push_back(i);
      //      pts.z.push_back(i);
      pts = DequePoints{i};
      // Add a the cropped building points to the collection
      part_count++;
      if (part_count == NR_PTS_PER_BUILDING) {
        part_count = 0;
        queue_cropped.push(pts);
        if (total_building_count % EMIT_TRACE_AT == 0) {
          logger_crop->trace(total_building_count);
        }
        total_building_count++;
        pts = DequePoints{};
      }
    }
    logger_crop->trace(total_building_count);
  }
}
