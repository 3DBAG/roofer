#pragma once

#include <filesystem>
#include <fstream>
#include <memory>

#include <roofer/io/CityJsonWriter.hpp>
#include <roofer/io/SpatialReferenceSystem.hpp>
#include <roofer/misc/projHelper.hpp>
#include <roofer/common/datastructures.hpp>
#include <roofer/logger/logger.h>

#include "types.hpp"

namespace fs = std::filesystem;

// Forward declaration to avoid including config.hpp in header
struct RooferConfig;

/**
 * @brief Handles serialization of BuildingTile objects to CityJSON format
 */
class BuildingTileSerializer {
 public:
  /**
   * @brief Constructor
   * @param config Reference to the configuration
   * @param srs Spatial reference system for the output
   */
  BuildingTileSerializer(const RooferConfig& config,
                         roofer::io::SpatialReferenceSystemInterface* srs);

  /**
   * @brief Serialize a BuildingTile to CityJSON format
   * @param building_tile The tile to serialize
   * @return true if serialization succeeded, false otherwise
   */
  bool serialize(BuildingTile& building_tile);

 private:
  const RooferConfig& config_;
  roofer::io::SpatialReferenceSystemInterface* srs_;
  roofer::logger::Logger& logger_;

  /**
   * @brief Setup the CityJsonWriter with configuration parameters
   * @param writer The writer to configure
   * @param building_tile The building tile being serialized
   */
  void configureCityJsonWriter(
      std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
      const BuildingTile& building_tile);

  /**
   * @brief Create attributes for the reconstruction time
   * @param building_tile The building tile to process
   */
  void createTimeAttributes(BuildingTile& building_tile);

  /**
   * @brief Write metadata if required
   * @param writer The CityJSON writer
   * @param ofs The output stream
   * @param building_tile The building tile
   */
  void writeMetadata(
      std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
      std::ofstream& ofs, const BuildingTile& building_tile);

  /**
   * @brief Serialize a single building
   * @param building The building to serialize
   * @param building_tile The parent tile
   * @param writer The CityJSON writer
   * @param ofs The output stream
   * @return true if successful, false otherwise
   */
  bool serializeBuilding(
      BuildingObject& building, BuildingTile& building_tile,
      std::unique_ptr<roofer::io::CityJsonWriterInterface>& writer,
      std::ofstream& ofs);

  /**
   * @brief Setup attribute row for a building
   * @param building The building
   * @param building_tile The parent tile
   * @return The configured attribute row
   */
  roofer::AttributeMapRow setupBuildingAttributes(
      const BuildingObject& building, BuildingTile& building_tile);

  /**
   * @brief Insert basic (non-optional) attributes
   * @param accessor The attribute accessor
   * @param building The building object
   */
  void insertBasicAttributes(roofer::AttributeMapRow& accessor,
                             const BuildingObject& building);

  /**
   * @brief Insert optional attributes
   * @param accessor The attribute accessor
   * @param building The building object
   */
  void insertOptionalAttributes(roofer::AttributeMapRow& accessor,
                                const BuildingObject& building);

  /**
   * @brief Insert extrusion mode attribute (enum to string)
   * @param accessor The attribute accessor
   * @param building The building object
   */
  void insertExtrusionModeAttribute(roofer::AttributeMapRow& accessor,
                                    const BuildingObject& building);

  /**
   * @brief Process LOD-specific attributes
   * @param accessor The attribute accessor
   * @param building The building object
   */
  void processLodAttributes(roofer::AttributeMapRow& accessor,
                            const BuildingObject& building);

  /**
   * @brief Get the output path for a tile
   * @param building_tile The building tile
   * @return The output file path
   */
  fs::path getOutputPath(const BuildingTile& building_tile);
};
