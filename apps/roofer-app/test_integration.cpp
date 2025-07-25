#include "BuildingFeature.hpp"
#include "BuildingFeatureMigrator.hpp"
#include "config.hpp"
#include <iostream>

/**
 * @brief Simple test to verify BuildingFeature integration
 */
int main() {
  std::cout << "Testing BuildingFeature integration..." << std::endl;

  try {
    // Create a simple BuildingFeature
    BuildingFeature feature;
    feature.h_ground = 10.5f;
    feature.setAttribute("test_attribute", 42);
    feature.setSuccess(true);

    std::cout << "✓ Created BuildingFeature with attributes" << std::endl;

    // Test attribute access
    auto test_attr = feature.getAttribute<int>("test_attribute");
    auto success = feature.getSuccess();

    if (test_attr.has_value() && test_attr.value() == 42 && success) {
      std::cout << "✓ Attribute access works correctly" << std::endl;
    } else {
      std::cout << "✗ Attribute access failed" << std::endl;
      return 1;
    }

    // Test collection
    BuildingFeatureCollection features;
    features.addFeature(feature);

    if (features.size() == 1) {
      std::cout << "✓ BuildingFeatureCollection works" << std::endl;
    } else {
      std::cout << "✗ BuildingFeatureCollection failed" << std::endl;
      return 1;
    }

    // Test migration (basic test)
    std::vector<BuildingObject> legacy_buildings;
    roofer::AttributeVecMap legacy_attributes;
    RooferConfig config;  // Default config

    BuildingFeatureMigrator::convertToLegacy(features, legacy_buildings,
                                             legacy_attributes, config);

    if (legacy_buildings.size() == 1) {
      std::cout << "✓ Migration to legacy format works" << std::endl;
    } else {
      std::cout << "✗ Migration to legacy format failed" << std::endl;
      return 1;
    }

    auto migrated_features = BuildingFeatureMigrator::convertFromLegacy(
        legacy_buildings, legacy_attributes, config);

    if (migrated_features.size() == 1) {
      std::cout << "✓ Migration from legacy format works" << std::endl;
    } else {
      std::cout << "✗ Migration from legacy format failed" << std::endl;
      return 1;
    }

    std::cout << "\n🎉 All BuildingFeature integration tests passed!"
              << std::endl;
    std::cout << "The new architecture is ready for use." << std::endl;

    return 0;

  } catch (const std::exception& e) {
    std::cout << "✗ Test failed with exception: " << e.what() << std::endl;
    return 1;
  }
}
