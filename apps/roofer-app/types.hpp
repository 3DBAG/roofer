#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <roofer/common/datastructures.hpp>
#include <roofer/misc/projHelper.hpp>

namespace fs = std::filesystem;

enum ExtrusionMode { STANDARD, LOD11_FALLBACK, SKIP };

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
  float z_offset = 0;

  std::unordered_map<int, roofer::Mesh> multisolids_lod12;
  std::unordered_map<int, roofer::Mesh> multisolids_lod13;
  std::unordered_map<int, roofer::Mesh> multisolids_lod22;

  size_t vector_source_index;
  bool reconstruction_success = false;
  int reconstruction_time = 0;

  // set in crop
  fs::path jsonl_path;
  float h_ground;       // without offset
  float h_pc_98p;       // without offset
  float h_pc_roof_70p;  // with offset!
  bool force_lod11;     // force_lod11 / fallback_lod11
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
  // bool was_skipped;  // b3_reconstructie_onvolledig;
};
