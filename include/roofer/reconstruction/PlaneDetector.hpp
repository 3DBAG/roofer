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

#include "cgal_shared_definitions.hpp"

namespace roofer::reconstruction {

  struct PlaneDetectorConfig {
    float horiz_min_count = 0.95;
    int metrics_normal_k = 5;
    int metrics_plane_k = 15;
    int metrics_plane_min_points = 20;
    float metrics_plane_epsilon = 0.2;
    float metrics_plane_normal_threshold = 0.75;
    float metrics_is_horizontal_threshold = 0.995;
    float metrics_probability_ransac = 0.05;
    float metrics_cluster_epsilon_ransac = 0.3;
    float metrics_is_wall_threshold = 0.3;
    int n_refit = 5;
    bool use_ransac = false;
    // float roof_percentile=0.5;

    // plane regularisation
    float maximum_angle_ = 25;
    float maximum_offset_ = 0.5;
    bool regularize_parallelism_ = false;
    bool regularize_orthogonality_ = false;
    bool regularize_coplanarity_ = false;
    bool regularize_axis_symmetry_ = false;
  };

  struct PlaneDetectorInterface {
    vec1i plane_id;
    IndexedPlanesWithPoints pts_per_roofplane;
    std::map<size_t, std::map<size_t, size_t> > plane_adjacencies;

    size_t horiz_roofplane_cnt = 0;
    size_t slant_roofplane_cnt = 0;
    size_t horiz_pt_cnt = 0, total_pt_cnt = 0, wall_pt_cnt = 0,
           unsegmented_pt_cnt = 0, total_plane_cnt = 0;

    std::string roof_type;
    float roof_elevation_70p;
    float roof_elevation_50p;
    float roof_elevation_min;
    float roof_elevation_max;

    virtual ~PlaneDetectorInterface() = default;
    virtual void detect(const PointCollection& points,
                        PlaneDetectorConfig config = PlaneDetectorConfig()) = 0;
  };

  std::unique_ptr<PlaneDetectorInterface> createPlaneDetector();

  struct ShapeDetectorInterface {
    virtual unsigned detectPlanes(PointCollection& point_collection,
                                  vec3f& normals, vec1i& labels,
                                  float probability = 0.01, int min_points = 15,
                                  float epsilon = 0.2,
                                  float cluster_epsilon = 0.5,
                                  float normal_threshold = 0.8) = 0;
  };

  std::unique_ptr<ShapeDetectorInterface> createShapeDetector();

}  // namespace roofer::reconstruction
