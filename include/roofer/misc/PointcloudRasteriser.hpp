#pragma once

#include <memory>
#include <roofer/common/common.hpp>
#include <roofer/common/datastructures.hpp>
#include <unordered_map>

namespace roofer::misc {

  void RasterisePointcloud(PointCollection& pointcloud, LinearRing& footprint,
                           ImageMap& image_bundle,
                           // Raster& heightfield,
                           float cellsize = 0.5);

  void gridthinPointcloud(PointCollection& pointcloud, const Image& cnt_image,
                          float max_density = 20);

  float computeNoDataFraction(const ImageMap& image_bundle);

  /**
   * @brief Assesses if the point cloud presents a glass roof based on the
   * presence  of ground points and non-ground points.
   *
   * This function analyzes the raster layer 'grp' provided in the ImageMap to
   * determine if its characteristics suggest the presence of a glass roof.
   * Each cell in the layer 'grp' gives probability for it to be a glass roof
   * cell The closer the value is to 1, the cell contains mostly points
   * belonging to one class, either ground or non-ground. The closer the value
   * is to 0, the cell contains a mix of points from both classes, which is a
   * characteristic of a glass roof. The function computes the average and the
   * mean value of the 'grp' layer and if they are below the threshold values,
   * the point cloud is classified as a glass roof.
   *
   * @param image_bundle The ImageMap containing multiple raster layers,
   * including 'grp'.
   * @return bool Returns true if the analysis suggests the presence of a glass
   * roof; otherwise, false.
   */
  bool assessGlassRoof(const ImageMap& pc);

  float computePointDensity(const ImageMap& pc);

  // Determine if the two point clouds describe the same object.
  bool isMutated(const ImageMap& a, const ImageMap& b,
                 const float& threshold_mutation_fraction,
                 const float& threshold_mutation_difference);

}  // namespace roofer::misc
