#include "BuildingTileSerializer.hpp"
#include <fmt/format.h>

// Include config only in implementation to avoid multiple definition issues
#include "config.hpp"

BuildingTileSerializer::BuildingTileSerializer(
    const RooferConfig& config,
    roofer::io::SpatialReferenceSystemInterface* srs)
    : config_(config),
      srs_(srs),
      logger_(roofer::logger::Logger::get_logger()) {}

bool BuildingTileSerializer::serialize(BuildingTile& building_tile) {
  logger_.info("[serializer] Serializing tile {} with {} buildings",
               building_tile.id, building_tile.buildings.size());

  try {
    // Create status attribute
    building_tile.attributes.maybe_insert_vec<bool>(config_.a_success);

    // Create time attributes
    createTimeAttributes(building_tile);

    // Setup CityJSON writer
    auto cityJsonWriter =
        roofer::io::createCityJsonWriter(*building_tile.proj_helper);
    configureCityJsonWriter(cityJsonWriter, building_tile);

    // Handle output file setup
    auto output_path = getOutputPath(building_tile);
    fs::create_directories(output_path.parent_path());
    std::ofstream ofs(output_path);

    if (!config_.omit_metadata) {
      writeMetadata(cityJsonWriter, ofs, building_tile);
    }

    // Serialize each building
    bool all_succeeded = true;
    for (auto& building : building_tile.buildings) {
      if (!serializeBuilding(building, building_tile, cityJsonWriter, ofs)) {
        all_succeeded = false;
      }
    }

    ofs.close();
    return all_succeeded;

  } catch (const std::exception& e) {
    logger_.error("[serializer] Failed to serialize tile {}: {}",
                  building_tile.id, e.what());
    return false;
  }
}

void BuildingTileSerializer::configureCityJsonWriter(
    std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
    const BuildingTile& building_tile) {
  writer->written_features_count = building_tile.buildings.size();
  writer->identifier_attribute = config_.id_attribute;

  // Configure translation/offset
  if (config_.cj_translate.has_value()) {
    writer->translate_x_ = (*config_.cj_translate)[0];
    writer->translate_y_ = (*config_.cj_translate)[1];
    writer->translate_z_ = (*config_.cj_translate)[2];
  } else if (building_tile.proj_helper->data_offset.has_value()) {
    writer->translate_x_ = (*building_tile.proj_helper->data_offset)[0];
    writer->translate_y_ = (*building_tile.proj_helper->data_offset)[1];
    writer->translate_z_ = (*building_tile.proj_helper->data_offset)[2];
  } else {
    throw std::runtime_error(
        fmt::format("Tile {} has no data offset, cannot write to cityjson",
                    building_tile.id));
  }

  // Configure scale
  writer->scale_x_ = config_.cj_scale[0];
  writer->scale_y_ = config_.cj_scale[1];
  writer->scale_z_ = config_.cj_scale[2];
}

void BuildingTileSerializer::createTimeAttributes(BuildingTile& building_tile) {
  auto attr_time = building_tile.attributes.maybe_insert_vec<int>(
      config_.a_reconstruction_time);
  if (attr_time.has_value()) {
    // Find the size from any existing attribute vector to determine original
    // footprint count
    size_t original_footprint_count = 0;

    // Try to get the size from any existing attribute vector
    const auto& attrs = building_tile.attributes.get_attributes();
    if (!attrs.empty()) {
      // Use the first attribute vector to determine the count
      std::visit(
          [&original_footprint_count](const auto& vec) {
            original_footprint_count = vec.size();
          },
          attrs.begin()->second);
    } else {
      // Fallback: use the number of buildings (this assumes 1:1 mapping)
      original_footprint_count = building_tile.buildings.size();
    }

    // Resize the time attributes vector to match the original footprint count
    attr_time->get().resize(original_footprint_count);

    // Set time attributes using the original vector source index from each
    // building
    for (const auto& building : building_tile.buildings) {
      if (building.vector_source_index < attr_time->get().size()) {
        attr_time->get()[building.vector_source_index] =
            building.reconstruction_time;
      }
    }
  }
  // Note: vector_source_index should remain as set in crop_tile (original
  // footprint index)
}

void BuildingTileSerializer::writeMetadata(
    std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
    std::ofstream& ofs, const BuildingTile& building_tile) {
  writer->write_metadata(ofs, srs_, building_tile.extent,
                         {.identifier = std::to_string(building_tile.id)});
}

bool BuildingTileSerializer::serializeBuilding(
    BuildingObject& building, BuildingTile& building_tile,
    std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
    std::ofstream& ofs) {
  try {
    // Setup building attributes
    auto accessor = setupBuildingAttributes(building, building_tile);

    // Prepare mesh pointers for different LODs
    std::unordered_map<int, roofer::Mesh>* ms12 = nullptr;
    std::unordered_map<int, roofer::Mesh>* ms13 = nullptr;
    std::unordered_map<int, roofer::Mesh>* ms22 = nullptr;

    if (config_.lod_12) {
      ms12 = &building.multisolids_lod12;
    }
    if (config_.lod_13) {
      ms13 = &building.multisolids_lod13;
    }
    if (config_.lod_22) {
      ms22 = &building.multisolids_lod22;
    }

    // Lift lod 0 footprint to ground height
    building.footprint.set_z(building.h_ground);

    // Write the feature
    writer->write_feature(ofs, building.footprint, ms12, ms13, ms22, accessor);

    return true;

  } catch (const std::exception& e) {
    logger_.error("[serializer] Failed to serialize building: {}", e.what());
    return false;
  }
}

roofer::AttributeMapRow BuildingTileSerializer::setupBuildingAttributes(
    const BuildingObject& building, BuildingTile& building_tile) {
  auto accessor = roofer::AttributeMapRow(building_tile.attributes,
                                          building.vector_source_index);

  // Basic attributes (non-optional)
  insertBasicAttributes(accessor, building);

  // Optional attributes
  insertOptionalAttributes(accessor, building);

  // Handle special case: extrusion mode (enum to string conversion)
  insertExtrusionModeAttribute(accessor, building);

  // Handle LOD-specific attributes
  processLodAttributes(accessor, building);

  return accessor;
}

void BuildingTileSerializer::insertBasicAttributes(
    roofer::AttributeMapRow& accessor, const BuildingObject& building) {
  if (!config_.a_h_ground.empty())
    accessor.insert(config_.a_h_ground, building.h_ground);
  if (!config_.a_h_pc_98p.empty())
    accessor.insert(config_.a_h_pc_98p, building.h_pc_98p);
  if (!config_.a_is_glass_roof.empty())
    accessor.insert(config_.a_is_glass_roof, building.is_glass_roof);
  if (!config_.a_pointcloud_unusable.empty())
    accessor.insert(config_.a_pointcloud_unusable,
                    building.pointcloud_insufficient);
  if (!config_.a_roof_type.empty())
    accessor.insert(config_.a_roof_type, building.roof_type);
}

void BuildingTileSerializer::insertOptionalAttributes(
    roofer::AttributeMapRow& accessor, const BuildingObject& building) {
  if (!config_.a_h_roof_50p.empty())
    accessor.insert_optional(config_.a_h_roof_50p, building.roof_elevation_50p);
  if (!config_.a_h_roof_70p.empty())
    accessor.insert_optional(config_.a_h_roof_70p, building.roof_elevation_70p);
  if (!config_.a_h_roof_min.empty())
    accessor.insert_optional(config_.a_h_roof_min, building.roof_elevation_min);
  if (!config_.a_h_roof_max.empty())
    accessor.insert_optional(config_.a_h_roof_max, building.roof_elevation_max);
  if (!config_.a_h_roof_ridge.empty())
    accessor.insert_optional(config_.a_h_roof_ridge,
                             building.roof_elevation_ridge);
  if (!config_.a_roof_n_planes.empty())
    accessor.insert_optional(config_.a_roof_n_planes, building.roof_n_planes);
  if (!config_.a_roof_n_ridgelines.empty())
    accessor.insert_optional(config_.a_roof_n_ridgelines,
                             building.roof_n_ridgelines);
}

void BuildingTileSerializer::insertExtrusionModeAttribute(
    roofer::AttributeMapRow& accessor, const BuildingObject& building) {
  if (!config_.a_extrusion_mode.empty()) {
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
      default:
        extrusion_mode_str = "unknown";
        break;
    }
    accessor.insert(config_.a_extrusion_mode, extrusion_mode_str);
  }
}

void BuildingTileSerializer::processLodAttributes(
    roofer::AttributeMapRow& accessor, const BuildingObject& building) {
  if (config_.lod_12) {
    if (!config_.a_rmse_lod12.empty())
      accessor.insert_optional(config_.a_rmse_lod12, building.rmse_lod12);
    if (!config_.a_volume_lod12.empty())
      accessor.insert_optional(config_.a_volume_lod12, building.volume_lod12);
#if RF_USE_VAL3DITY
    if (!config_.a_val3dity_lod12.empty())
      accessor.insert_optional(config_.a_val3dity_lod12,
                               building.val3dity_lod12);
#endif
  }

  if (config_.lod_13) {
    if (!config_.a_rmse_lod13.empty())
      accessor.insert_optional(config_.a_rmse_lod13, building.rmse_lod13);
    if (!config_.a_volume_lod13.empty())
      accessor.insert_optional(config_.a_volume_lod13, building.volume_lod13);
#if RF_USE_VAL3DITY
    if (!config_.a_val3dity_lod13.empty())
      accessor.insert_optional(config_.a_val3dity_lod13,
                               building.val3dity_lod13);
#endif
  }

  if (config_.lod_22) {
    if (!config_.a_rmse_lod22.empty())
      accessor.insert_optional(config_.a_rmse_lod22, building.rmse_lod22);
    if (!config_.a_volume_lod22.empty())
      accessor.insert_optional(config_.a_volume_lod22, building.volume_lod22);
#if RF_USE_VAL3DITY
    if (!config_.a_val3dity_lod22.empty())
      accessor.insert_optional(config_.a_val3dity_lod22,
                               building.val3dity_lod22);
#endif
  }
}

fs::path BuildingTileSerializer::getOutputPath(
    const BuildingTile& building_tile) {
  // Create filename based on tile extent
  int minx = static_cast<int>(building_tile.extent.min()[0]);
  int miny = static_cast<int>(building_tile.extent.min()[1]);
  return fs::path(config_.output_path) /
         fmt::format("{:06d}_{:06d}.city.jsonl", minx, miny);
}
