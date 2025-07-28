#pragma once

#include "BuildingFeature.hpp"
#include "config.hpp"
#include <roofer/io/CityJsonWriter.hpp>
#include <roofer/misc/projHelper.hpp>
#include <roofer/logger/logger.h>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

/**
 * @brief Simplified serializer for BuildingFeature objects
 *
 * This class directly serializes BuildingFeature objects to CityJSON format,
 * eliminating the need for complex attribute mapping between BuildingObject
 * and AttributeVecMap. Much cleaner and more maintainable than the legacy
 * approach.
 */
class BuildingFeatureSerializer {
 private:
  const RooferConfig& config_;
  roofer::io::SpatialReferenceSystemInterface* srs_;
  roofer::logger::Logger logger_;

 public:
  BuildingFeatureSerializer(const RooferConfig& config,
                            roofer::io::SpatialReferenceSystemInterface* srs);

  /**
   * @brief Serialize a collection of BuildingFeatures to CityJSON
   *
   * @param features The BuildingFeatureCollection to serialize
   * @param proj_helper The projection helper for coordinate transformations
   * @return true if serialization succeeded, false otherwise
   */
  bool serialize(BuildingFeatureCollection& features,
                 roofer::misc::projHelperInterface& proj_helper);

 private:
  /**
   * @brief Setup CityJSON writer with configuration
   */
  void configureCityJsonWriter(
      std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
      size_t buildings_count, roofer::misc::projHelperInterface& proj_helper);

  /**
   * @brief Serialize a single BuildingFeature
   */
  bool serializeFeature(
      BuildingFeature& feature,
      std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
      std::ofstream& ofs);

  /**
   * @brief Create an attribute accessor for the feature
   *
   * This creates an AttributeMapRow-compatible interface for the CityJSON
   * writer without needing a separate AttributeVecMap.
   */
  roofer::AttributeMapRow createAttributeAccessor(
      const BuildingFeature& feature);

  /**
   * @brief Get the output file path
   */
  fs::path getOutputPath();
};
