#include "BuildingFeatureSerializer.hpp"
#include <fmt/format.h>

BuildingFeatureSerializer::BuildingFeatureSerializer(
    const RooferConfig& config,
    roofer::io::SpatialReferenceSystemInterface* srs)
    : config_(config),
      srs_(srs),
      logger_(roofer::logger::Logger::get_logger()) {}

bool BuildingFeatureSerializer::serialize(
    BuildingFeatureCollection& features,
    roofer::misc::projHelperInterface& proj_helper) {
  logger_.info("[feature-serializer] Serializing {} building features",
               features.size());

  try {
    // Setup CityJSON writer
    auto cityJsonWriter = roofer::io::createCityJsonWriter(proj_helper);
    configureCityJsonWriter(cityJsonWriter, features.size(), proj_helper);

    // Handle output file setup
    auto output_path = getOutputPath();
    fs::create_directories(output_path.parent_path());
    std::ofstream ofs(output_path);

    // Serialize each building feature
    bool all_succeeded = true;
    for (auto& feature_ptr : features) {
      if (!serializeFeature(*feature_ptr, cityJsonWriter, ofs)) {
        all_succeeded = false;
      }
    }

    ofs.close();
    return all_succeeded;

  } catch (const std::exception& e) {
    logger_.error(
        "[feature-serializer] Failed to serialize building features: {}",
        e.what());
    return false;
  }
}

void BuildingFeatureSerializer::configureCityJsonWriter(
    std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
    size_t buildings_count, roofer::misc::projHelperInterface& proj_helper) {
  writer->written_features_count = buildings_count;
  writer->identifier_attribute = config_.id_attribute;

  // Configure translation/offset
  if (config_.cj_translate.has_value()) {
    writer->translate_x_ = (*config_.cj_translate)[0];
    writer->translate_y_ = (*config_.cj_translate)[1];
    writer->translate_z_ = (*config_.cj_translate)[2];
  } else if (proj_helper.data_offset.has_value()) {
    writer->translate_x_ = (*proj_helper.data_offset)[0];
    writer->translate_y_ = (*proj_helper.data_offset)[1];
    writer->translate_z_ = (*proj_helper.data_offset)[2];
  } else {
    throw std::runtime_error(
        "No data offset available, cannot write to cityjson");
  }

  // Configure scale
  writer->scale_x_ = config_.cj_scale[0];
  writer->scale_y_ = config_.cj_scale[1];
  writer->scale_z_ = config_.cj_scale[2];
}

bool BuildingFeatureSerializer::serializeFeature(
    BuildingFeature& feature,
    std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
    std::ofstream& ofs) {
  try {
    // Create attribute accessor directly from the feature's attributes
    auto accessor = createAttributeAccessor(feature);

    // Prepare mesh pointers for different LODs
    std::unordered_map<int, roofer::Mesh>* ms12 = nullptr;
    std::unordered_map<int, roofer::Mesh>* ms13 = nullptr;
    std::unordered_map<int, roofer::Mesh>* ms22 = nullptr;

    if (config_.lod_12) {
      ms12 = &feature.multisolids_lod12;
    }
    if (config_.lod_13) {
      ms13 = &feature.multisolids_lod13;
    }
    if (config_.lod_22) {
      ms22 = &feature.multisolids_lod22;
    }

    // Lift lod 0 footprint to ground height
    feature.footprint.set_z(feature.h_ground);

    // Write the feature
    writer->write_feature(ofs, feature.footprint, ms12, ms13, ms22, accessor);

    return true;

  } catch (const std::exception& e) {
    logger_.error(
        "[feature-serializer] Failed to serialize building feature: {}",
        e.what());
    return false;
  }
}

roofer::AttributeMapRow BuildingFeatureSerializer::createAttributeAccessor(
    const BuildingFeature& feature) {
  // Create an AttributeMapRow directly from the feature's attributes
  roofer::AttributeMapRow accessor;

  // Copy all attributes from the feature
  for (const auto& [name, value] : feature.getAttributes()) {
    // Convert from BuildingFeature::AttributeValue to AttributeMapRow format
    std::visit(
        [&](const auto& val) {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<T, std::monostate>) {
            accessor.set_null(name);
          } else {
            accessor.insert(name, val);
          }
        },
        value);
  }

  // Ensure critical attributes are present (with fallbacks from BuildingObject
  // properties)

  // Basic building properties
  if (!accessor.has_name(config_.a_h_ground) && !config_.a_h_ground.empty()) {
    accessor.insert(config_.a_h_ground, feature.h_ground);
  }
  if (!accessor.has_name(config_.a_h_pc_98p) && !config_.a_h_pc_98p.empty()) {
    accessor.insert(config_.a_h_pc_98p, feature.h_pc_98p);
  }
  if (!accessor.has_name(config_.a_is_glass_roof) &&
      !config_.a_is_glass_roof.empty()) {
    accessor.insert(config_.a_is_glass_roof, feature.is_glass_roof);
  }
  if (!accessor.has_name(config_.a_pointcloud_unusable) &&
      !config_.a_pointcloud_unusable.empty()) {
    accessor.insert(config_.a_pointcloud_unusable,
                    feature.pointcloud_insufficient);
  }
  if (!accessor.has_name(config_.a_roof_type) && !config_.a_roof_type.empty()) {
    accessor.insert(config_.a_roof_type, feature.roof_type);
  }

  // Reconstruction metadata
  if (!accessor.has_name(config_.a_success) && !config_.a_success.empty()) {
    accessor.insert(config_.a_success, feature.reconstruction_success);
  }
  if (!accessor.has_name(config_.a_reconstruction_time) &&
      !config_.a_reconstruction_time.empty()) {
    accessor.insert(config_.a_reconstruction_time, feature.reconstruction_time);
  }

  // Optional attributes with fallbacks
  if (!accessor.has_name(config_.a_h_roof_50p) &&
      !config_.a_h_roof_50p.empty()) {
    if (feature.roof_elevation_50p.has_value()) {
      accessor.insert(config_.a_h_roof_50p, feature.roof_elevation_50p.value());
    } else {
      accessor.set_null(config_.a_h_roof_50p);
    }
  }
  if (!accessor.has_name(config_.a_h_roof_70p) &&
      !config_.a_h_roof_70p.empty()) {
    if (feature.roof_elevation_70p.has_value()) {
      accessor.insert(config_.a_h_roof_70p, feature.roof_elevation_70p.value());
    } else {
      accessor.set_null(config_.a_h_roof_70p);
    }
  }
  if (!accessor.has_name(config_.a_h_roof_min) &&
      !config_.a_h_roof_min.empty()) {
    if (feature.roof_elevation_min.has_value()) {
      accessor.insert(config_.a_h_roof_min, feature.roof_elevation_min.value());
    } else {
      accessor.set_null(config_.a_h_roof_min);
    }
  }
  if (!accessor.has_name(config_.a_h_roof_max) &&
      !config_.a_h_roof_max.empty()) {
    if (feature.roof_elevation_max.has_value()) {
      accessor.insert(config_.a_h_roof_max, feature.roof_elevation_max.value());
    } else {
      accessor.set_null(config_.a_h_roof_max);
    }
  }
  if (!accessor.has_name(config_.a_h_roof_ridge) &&
      !config_.a_h_roof_ridge.empty()) {
    if (feature.roof_elevation_ridge.has_value()) {
      accessor.insert(config_.a_h_roof_ridge,
                      feature.roof_elevation_ridge.value());
    } else {
      accessor.set_null(config_.a_h_roof_ridge);
    }
  }
  if (!accessor.has_name(config_.a_roof_n_planes) &&
      !config_.a_roof_n_planes.empty()) {
    if (feature.roof_n_planes.has_value()) {
      accessor.insert(config_.a_roof_n_planes, feature.roof_n_planes.value());
    } else {
      accessor.set_null(config_.a_roof_n_planes);
    }
  }
  if (!accessor.has_name(config_.a_roof_n_ridgelines) &&
      !config_.a_roof_n_ridgelines.empty()) {
    if (feature.roof_n_ridgelines.has_value()) {
      accessor.insert(config_.a_roof_n_ridgelines,
                      feature.roof_n_ridgelines.value());
    } else {
      accessor.set_null(config_.a_roof_n_ridgelines);
    }
  }

  // Extrusion mode (using BuildingFeature's built-in enum conversion)
  if (!accessor.has_name(config_.a_extrusion_mode) &&
      !config_.a_extrusion_mode.empty()) {
    accessor.insert(config_.a_extrusion_mode, feature.getExtrusionMode());
  }

  // LOD-specific attributes
  if (config_.lod_12) {
    if (!accessor.has_name(config_.a_rmse_lod12) &&
        !config_.a_rmse_lod12.empty()) {
      if (feature.rmse_lod12.has_value()) {
        accessor.insert(config_.a_rmse_lod12, feature.rmse_lod12.value());
      } else {
        accessor.set_null(config_.a_rmse_lod12);
      }
    }
    if (!accessor.has_name(config_.a_volume_lod12) &&
        !config_.a_volume_lod12.empty()) {
      if (feature.volume_lod12.has_value()) {
        accessor.insert(config_.a_volume_lod12, feature.volume_lod12.value());
      } else {
        accessor.set_null(config_.a_volume_lod12);
      }
    }
#if RF_USE_VAL3DITY
    if (!accessor.has_name(config_.a_val3dity_lod12) &&
        !config_.a_val3dity_lod12.empty()) {
      if (feature.val3dity_lod12.has_value()) {
        accessor.insert(config_.a_val3dity_lod12,
                        feature.val3dity_lod12.value());
      } else {
        accessor.set_null(config_.a_val3dity_lod12);
      }
    }
#endif
  }

  if (config_.lod_13) {
    if (!accessor.has_name(config_.a_rmse_lod13) &&
        !config_.a_rmse_lod13.empty()) {
      if (feature.rmse_lod13.has_value()) {
        accessor.insert(config_.a_rmse_lod13, feature.rmse_lod13.value());
      } else {
        accessor.set_null(config_.a_rmse_lod13);
      }
    }
    if (!accessor.has_name(config_.a_volume_lod13) &&
        !config_.a_volume_lod13.empty()) {
      if (feature.volume_lod13.has_value()) {
        accessor.insert(config_.a_volume_lod13, feature.volume_lod13.value());
      } else {
        accessor.set_null(config_.a_volume_lod13);
      }
    }
#if RF_USE_VAL3DITY
    if (!accessor.has_name(config_.a_val3dity_lod13) &&
        !config_.a_val3dity_lod13.empty()) {
      if (feature.val3dity_lod13.has_value()) {
        accessor.insert(config_.a_val3dity_lod13,
                        feature.val3dity_lod13.value());
      } else {
        accessor.set_null(config_.a_val3dity_lod13);
      }
    }
#endif
  }

  if (config_.lod_22) {
    if (!accessor.has_name(config_.a_rmse_lod22) &&
        !config_.a_rmse_lod22.empty()) {
      if (feature.rmse_lod22.has_value()) {
        accessor.insert(config_.a_rmse_lod22, feature.rmse_lod22.value());
      } else {
        accessor.set_null(config_.a_rmse_lod22);
      }
    }
    if (!accessor.has_name(config_.a_volume_lod22) &&
        !config_.a_volume_lod22.empty()) {
      if (feature.volume_lod22.has_value()) {
        accessor.insert(config_.a_volume_lod22, feature.volume_lod22.value());
      } else {
        accessor.set_null(config_.a_volume_lod22);
      }
    }
#if RF_USE_VAL3DITY
    if (!accessor.has_name(config_.a_val3dity_lod22) &&
        !config_.a_val3dity_lod22.empty()) {
      if (feature.val3dity_lod22.has_value()) {
        accessor.insert(config_.a_val3dity_lod22,
                        feature.val3dity_lod22.value());
      } else {
        accessor.set_null(config_.a_val3dity_lod22);
      }
    }
#endif
  }

  return accessor;
}

fs::path BuildingFeatureSerializer::getOutputPath() {
  // Use a simple filename since we don't have tile concept anymore
  return fs::path(config_.output_path) / "buildings.city.jsonl";
}
