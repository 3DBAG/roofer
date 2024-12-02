// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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
   * @param threshold_glass_roof The threshold for determining if this building
   * has a glass roof.
   * @return bool Returns true if the analysis suggests the presence of a glass
   * roof; otherwise, false.
   */
  bool testForGlassRoof(const ImageMap& image_bundle,
                        float threshold_glass_roof = 0.75);

  float computePointDensity(const ImageMap& image_bundle);

  float computeRoofElevation(const ImageMap& image_bundle, float percentile);

  // Determine if the two point clouds describe the same object.
  bool isMutated(const ImageMap& a, const ImageMap& b,
                 const float& threshold_mutation_fraction,
                 const float& threshold_mutation_difference);

}  // namespace roofer::misc
