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
#include <roofer/common/datastructures.hpp>
#include <roofer/misc/projHelper.hpp>

namespace roofer::io {
  struct PointCloudCropperConfig {
    float cellsize = 50.0;
    float buffer = 1.0;
    float ground_percentile = 0.05;
    float max_density_delta = 0.05;
    float coverage_threshold = 2.0;
    int ground_class = 2;
    int building_class = 6;
    std::string wkt_ = "";
    bool handle_overlap_points = false;
    bool use_acquisition_year = true;
  };
  struct PointCloudCropperInterface {
    roofer::misc::projHelperInterface& pjHelper;

    PointCloudCropperInterface(roofer::misc::projHelperInterface& pjh)
        : pjHelper(pjh){};
    virtual ~PointCloudCropperInterface() = default;

    virtual void process(
        const std::vector<std::string>& lasfiles,
        std::vector<LinearRing>& polygons,
        std::vector<LinearRing>& buf_polygons,
        std::vector<PointCollection>& point_clouds, veco1f& ground_elevations,
        vec1i& acquisition_years, vec1b& pointcloud_insufficient,
        const Box& polygon_extent,
        PointCloudCropperConfig cfg = PointCloudCropperConfig{}) = 0;

    virtual float get_min_terrain_elevation() const = 0;
    // virtual void process(
    //     std::string filepaths, std::vector<LinearRing>& polygons,
    //     std::vector<LinearRing>& buf_polygons,
    //     std::vector<PointCollection>& point_clouds, vec1f& ground_elevations,
    //     vec1i& acquisition_years, const Box& polygon_extent,
    //     PointCloudCropperConfig cfg = PointCloudCropperConfig{}) = 0;
  };

  std::unique_ptr<PointCloudCropperInterface> createPointCloudCropper(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
