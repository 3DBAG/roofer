#pragma once

#include <memory>
#include <unordered_map>

#include <roofer/common/common.hpp>
#include <roofer/common/datastructures.hpp>

namespace roofer::misc {

  void RasterisePointcloud(
    PointCollection& pointcloud,
    LinearRing& footprint,
    ImageMap& image_bundle,
    // Raster& heightfield,
    float cellsize = 0.5
  );

  void gridthinPointcloud(PointCollection& pointcloud, const Image& cnt_image, float max_density=20);

  float computeNoDataFraction(const ImageMap& pc);

  float computePointDensity(const ImageMap& pc);

  // Determine if the two point clouds describe the same object.
  bool isMutated(const ImageMap& a, 
                 const ImageMap& b, 
                 const float& threshold_mutation_fraction,
                 const float& threshold_mutation_difference);

  
}