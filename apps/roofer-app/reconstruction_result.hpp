// Copyright (c) 2018-2025 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ravi Peters
// Balazs Dukai

#pragma once

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>

#include <roofer/misc/projHelper.hpp>
#include <roofer/reconstruction/Underpass.hpp>
#include <fmt/format.h>

enum ExtrusionMode { STANDARD, LOD11_FALLBACK, SKIP, FAIL };

/**
 * @brief A single building object
 *
 * Contains the footprint polygon, the point cloud, the reconstructed model and
 * some attributes that are set during the reconstruction.
 */
struct BuildingObject {
  roofer::PointCollection pointcloud_ground;
  roofer::PointCollection pointcloud_building;
  roofer::LinearRing footprint;
  std::string id;
  std::vector<roofer::Underpass> underpasses;
  roofer::SegmentCollection external_lines;
  float z_offset = 0;

  std::unordered_map<int, roofer::Mesh> multisolids_lod12;
  std::unordered_map<int, roofer::Mesh> multisolids_lod13;
  std::unordered_map<int, roofer::Mesh> multisolids_lod22;

  size_t attribute_index;
  bool reconstruction_success = false;
  int reconstruction_time = 0;

  // set in crop
  std::filesystem::path jsonl_path;
  std::optional<float> h_ground;  // without offset
  float h_pc_98p;                 // without offset
  float h_pc_roof_70p;            // with offset!
  bool force_lod11;               // force_lod11 / fallback_lod11
  bool pointcloud_insufficient;
  bool is_glass_roof;
  std::optional<float> roof_h_fallback;
  ExtrusionMode extrusion_mode = STANDARD;

  // set in reconstruction
  // optionals may not get assigned a valid value
  std::string roof_type = "unknown";
  std::optional<float> roof_elevation_50p;
  std::optional<float> roof_elevation_70p;
  std::optional<float> roof_elevation_min;
  std::optional<float> roof_elevation_max;
  std::optional<float> roof_elevation_ridge;
  std::optional<int> roof_n_planes;
  std::optional<float> rmse_lod12;
  std::optional<float> rmse_lod13;
  std::optional<float> rmse_lod22;
  std::optional<float> volume_lod12;
  std::optional<float> volume_lod13;
  std::optional<float> volume_lod22;
  std::optional<int> roof_n_ridgelines;
  std::optional<std::string> val3dity_lod12;
  std::optional<std::string> val3dity_lod13;
  std::optional<std::string> val3dity_lod22;
  std::string output_label() const {
    return jsonl_path.empty() ? id : jsonl_path.string();
  }
};

struct TerrainData {
  std::vector<std::vector<roofer::LinearRing>> components;
  roofer::AttributeMapRow attributes;
};

/**
 * @brief Indicates the progress of the object through the whole roofer process.
 */
enum Progress : std::uint8_t {
  CROP_NOT_STARTED,
  CROP_IN_PROGRESS,
  CROP_SUCCEEDED,
  CROP_FAILED,
  RECONSTRUCTION_IN_PROGRESS,
  RECONSTRUCTION_SUCCEEDED,
  RECONSTRUCTION_FAILED,
  SERIALIZATION_IN_PROGRESS,
  SERIALIZATION_SUCCEEDED,
  SERIALIZATION_FAILED,
};

inline auto format_as(Progress p) { return fmt::underlying(p); }

/**
 * @brief A single batch for processing
 *
 * It contains a tile ID, all the buildings of the tile, their attributes,
 * the progress of each building, the tile extent and projection information.
 * Buildings and attributes are stored in separate containers and they are
 * matched on their index in the container.
 */
struct BuildingTile {
  std::size_t id = 0;
  std::vector<BuildingObject> buildings;
  roofer::AttributeVecMap attributes;
  std::vector<Progress> buildings_progresses;
  std::size_t buildings_cnt = 0;
  // Only the completion consumer updates this count after submission.
  std::size_t remaining = 0;
  // offset
  std::unique_ptr<roofer::misc::projHelperInterface> proj_helper;
  // extent
  roofer::TBox<double> extent;
  std::optional<TerrainData> terrain;

  std::vector<std::pair<Progress, size_t>> count_progresses() const;
};

// Each task owns its building and keeps its tile's projection and attributes
// alive until the consumer serializes it or returns it to its original slot.
struct BuildingTask {
  std::shared_ptr<BuildingTile> tile;
  size_t building_idx;
  size_t submission_index;  // Stream IDs follow submission order.
  BuildingObject building;
};

class CompletedBuildings {
 public:
  explicit CompletedBuildings(size_t capacity) : capacity_(capacity) {}

  void push(BuildingTask building) {
    std::unique_lock lock(mutex_);
    space_.wait(lock, [&] { return closed_ || queue_.size() < capacity_; });
    if (closed_) return;
    queue_.push_back(std::move(building));
    ready_.notify_one();
  }

  std::optional<BuildingTask> next() {
    std::unique_lock lock(mutex_);
    ready_.wait(lock, [&] { return closed_ || !queue_.empty(); });
    if (queue_.empty()) return std::nullopt;
    auto result = std::move(queue_.front());
    queue_.pop_front();
    space_.notify_one();
    return result;
  }

  void close() {
    std::lock_guard lock(mutex_);
    closed_ = true;
    ready_.notify_one();
    space_.notify_all();
  }

 private:
  const size_t capacity_;
  std::deque<BuildingTask> queue_;
  std::mutex mutex_;
  std::condition_variable ready_, space_;
  bool closed_ = false;
};
