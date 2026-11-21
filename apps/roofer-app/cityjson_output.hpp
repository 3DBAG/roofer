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

#include <algorithm>
#include <atomic>
#include <fstream>
#include <roofer/io/CityJsonWriter.hpp>
#include "reconstruction_result.hpp"
#include "config.hpp"

inline auto make_cityjson_writer(roofer::misc::projHelperInterface& projection,
                                 const RooferConfig& cfg,
                                 const roofer::arr3d& translate) {
  auto writer = roofer::io::createCityJsonWriter(projection);
  writer->identifier_attribute = cfg.id_attribute;
  writer->translate_x_ = translate[0];
  writer->translate_y_ = translate[1];
  writer->translate_z_ = translate[2];
  writer->scale_x_ = cfg.cj_scale[0];
  writer->scale_y_ = cfg.cj_scale[1];
  writer->scale_z_ = cfg.cj_scale[2];
  return writer;
}

inline void write_building(roofer::io::CityJsonWriterInterface& writer,
                           std::ostream& out, BuildingObject& building,
                           BuildingTile& building_tile,
                           const RooferConfig& cfg) {
  auto attrow = roofer::AttributeMapRow(building_tile.attributes,
                                        building.attribute_index);

  if (!cfg.a_success.empty())
    attrow.insert(cfg.a_success, building.reconstruction_success);
  if (!cfg.a_reconstruction_time.empty())
    attrow.insert(cfg.a_reconstruction_time, building.reconstruction_time);
  if (!cfg.a_h_ground.empty())
    attrow.insert_optional(cfg.a_h_ground, building.h_ground);
  if (!cfg.a_h_pc_98p.empty()) attrow.insert(cfg.a_h_pc_98p, building.h_pc_98p);
  if (!cfg.a_is_glass_roof.empty())
    attrow.insert(cfg.a_is_glass_roof, building.is_glass_roof);
  if (!cfg.a_has_underpass.empty()) {
    bool has_underpass = false;
    for (const auto& [part, mesh] : building.multisolids_lod22)
      if (std::find(mesh.get_labels().begin(), mesh.get_labels().end(), 4) !=
          mesh.get_labels().end()) {
        has_underpass = true;
        break;
      }
    attrow.insert(cfg.a_has_underpass, has_underpass);
  }
  if (!cfg.a_pointcloud_unusable.empty())
    attrow.insert(cfg.a_pointcloud_unusable, building.pointcloud_insufficient);
  if (!cfg.a_roof_type.empty())
    attrow.insert(cfg.a_roof_type, building.roof_type);
  if (!cfg.a_h_roof_50p.empty())
    attrow.insert_optional(cfg.a_h_roof_50p, building.roof_elevation_50p);
  if (!cfg.a_h_roof_70p.empty())
    attrow.insert_optional(cfg.a_h_roof_70p, building.roof_elevation_70p);
  if (!cfg.a_h_roof_min.empty())
    attrow.insert_optional(cfg.a_h_roof_min, building.roof_elevation_min);
  if (!cfg.a_h_roof_max.empty())
    attrow.insert_optional(cfg.a_h_roof_max, building.roof_elevation_max);
  if (!cfg.a_h_roof_ridge.empty())
    attrow.insert_optional(cfg.a_h_roof_ridge, building.roof_elevation_ridge);
  if (!cfg.a_roof_n_planes.empty())
    attrow.insert_optional(cfg.a_roof_n_planes, building.roof_n_planes);
  if (!cfg.a_roof_n_ridgelines.empty())
    attrow.insert_optional(cfg.a_roof_n_ridgelines, building.roof_n_ridgelines);
  if (!cfg.a_extrusion_mode.empty()) {
    std::string extrusion_mode_str;
    switch (building.extrusion_mode) {
      case STANDARD:
        extrusion_mode_str = "standard";
        break;
      case LOD11_FALLBACK:
        extrusion_mode_str = "lod11_fallback";
        break;
      case SKIP:
        extrusion_mode_str = "skip";
        break;
      case FAIL:
        extrusion_mode_str = "fail";
        break;
      default:
        extrusion_mode_str = "unknown";
        break;
    }
    attrow.insert(cfg.a_extrusion_mode, extrusion_mode_str);
  }

  std::unordered_map<int, roofer::Mesh>* ms12 = nullptr;
  std::unordered_map<int, roofer::Mesh>* ms13 = nullptr;
  std::unordered_map<int, roofer::Mesh>* ms22 = nullptr;
  if (cfg.reconstruction.lod12) {
    ms12 = &building.multisolids_lod12;

    if (!cfg.a_rmse_lod12.empty())
      attrow.insert_optional(cfg.a_rmse_lod12, building.rmse_lod12);
    if (!cfg.a_volume_lod12.empty())
      attrow.insert_optional(cfg.a_volume_lod12, building.volume_lod12);
#if RF_USE_VAL3DITY
    if (!cfg.a_val3dity_lod12.empty())
      attrow.insert_optional(cfg.a_val3dity_lod12, building.val3dity_lod12);
#endif
  }
  if (cfg.reconstruction.lod13) {
    ms13 = &building.multisolids_lod13;
    if (!cfg.a_rmse_lod13.empty())
      attrow.insert_optional(cfg.a_rmse_lod13, building.rmse_lod13);
    if (!cfg.a_volume_lod13.empty())
      attrow.insert_optional(cfg.a_volume_lod13, building.volume_lod13);
#if RF_USE_VAL3DITY
    if (!cfg.a_val3dity_lod13.empty())
      attrow.insert_optional(cfg.a_val3dity_lod13, building.val3dity_lod13);
#endif
  }
  if (cfg.reconstruction.lod22) {
    ms22 = &building.multisolids_lod22;
    if (!cfg.a_rmse_lod22.empty())
      attrow.insert_optional(cfg.a_rmse_lod22, building.rmse_lod22);
    if (!cfg.a_volume_lod22.empty())
      attrow.insert_optional(cfg.a_volume_lod22, building.volume_lod22);
#if RF_USE_VAL3DITY
    if (!cfg.a_val3dity_lod22.empty())
      attrow.insert_optional(cfg.a_val3dity_lod22, building.val3dity_lod22);
#endif
  }
  // lift lod 0 footprint to h_ground
  if (building.h_ground.has_value()) {
    building.footprint.set_z(*building.h_ground);
  }
  writer.write_feature(out, building.footprint, ms12, ms13, ms22, attrow);
}

inline void stream_completed_buildings(
    CompletedBuildings& completed, std::ostream& out, const RooferConfig& cfg,
    const roofer::arr3d& translate,
    const roofer::io::SpatialReferenceSystemInterface& srs,
    std::atomic<size_t>& building_count, std::atomic<size_t>& tile_count) {
  if (!cfg.omit_metadata) {
    auto projection = roofer::misc::createProjHelper();
    auto writer = make_cityjson_writer(*projection, cfg, translate);
    writer->write_metadata(out, &srs, {},
                           {.include_geographical_extent = false});
  }

  while (auto result = completed.next()) {
    auto& tile = *result->tile;
    auto writer = make_cityjson_writer(*tile.proj_helper, cfg, translate);
    writer->written_features_count = result->submission_index;
    write_building(*writer, out, result->building, tile, cfg);
    ++building_count;
    if (--tile.remaining == 0) {
      if (tile.terrain) {
        writer->write_tin_relief_feature(
            out, fmt::format("terrain-{}", tile.id), tile.terrain->components,
            tile.terrain->attributes);
      }
      ++tile_count;
    }
  }
}

inline void write_tile_files(
    const RooferConfig& cfg, BuildingTile& building_tile,
    const roofer::io::SpatialReferenceSystemInterface& srs,
    std::atomic<size_t>& serialized_buildings_cnt) {
  auto& logger = roofer::logger::Logger::get_logger();
  auto writer = make_cityjson_writer(
      *building_tile.proj_helper, cfg,
      cfg.cj_translate ? *cfg.cj_translate
                       : building_tile.proj_helper->data_offset.value());
  writer->written_features_count = serialized_buildings_cnt;

  std::ofstream ofs;
  if (!cfg.split_cjseq) {
    // get bottom left corner coordinates
    int minx = building_tile.extent.min()[0];
    int miny = building_tile.extent.min()[1];
    auto jsonl_tile_path = fs::path(cfg.output_path) /
                           fmt::format("{:06d}_{:06d}.city.jsonl", minx, miny);
    fs::create_directories(jsonl_tile_path.parent_path());
    ofs.open(jsonl_tile_path);
    if (!cfg.omit_metadata)
      writer->write_metadata(ofs, &srs, building_tile.extent,
                             {.identifier = std::to_string(building_tile.id)});
  } else {
    if (!cfg.omit_metadata) {
      std::string metadata_json_file =
          fmt::format(fmt::runtime(cfg.metadata_json_file_spec),
                      fmt::arg("path", cfg.output_path));
      fs::create_directories(fs::path(metadata_json_file).parent_path());
      ofs.open(metadata_json_file);
      writer->write_metadata(ofs, &srs, building_tile.extent,
                             {.identifier = std::to_string(building_tile.id)});
      ofs.close();
    }
  }

  if (building_tile.terrain.has_value()) {
    const auto terrain_id = fmt::format("terrain-{}", building_tile.id);
    if (cfg.split_cjseq) {
      const int minx = building_tile.extent.min()[0];
      const int miny = building_tile.extent.min()[1];
      const auto terrain_path =
          fs::path(cfg.output_path) /
          fmt::format("{:06d}_{:06d}.terrain.city.jsonl", minx, miny);
      fs::create_directories(terrain_path.parent_path());
      std::ofstream terrain_ofs(terrain_path);
      writer->write_tin_relief_feature(terrain_ofs, terrain_id,
                                       building_tile.terrain->components,
                                       building_tile.terrain->attributes);
    } else {
      writer->write_tin_relief_feature(ofs, terrain_id,
                                       building_tile.terrain->components,
                                       building_tile.terrain->attributes);
    }
  }

  for (auto& building : building_tile.buildings) {
    if (cfg.split_cjseq) {
      fs::create_directories(building.jsonl_path.parent_path());
      ofs.open(building.jsonl_path);
    }
    try {
      write_building(*writer, ofs, building, building_tile, cfg);
      ++serialized_buildings_cnt;
    } catch (const std::exception& e) {
      if (!ofs) throw;
      logger.error("[serializer] Failed to serialize {}. {}",
                   building.output_label(), e.what());
    }
    if (cfg.split_cjseq) ofs.close();
  }
  if (!cfg.split_cjseq) {
    ofs.close();
  }
}
