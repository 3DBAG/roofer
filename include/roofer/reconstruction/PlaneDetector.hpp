// Copyright (c) 2018-2026 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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
#include <roofer/common/ConfigField.hpp>

#include "cgal_shared_definitions.hpp"

namespace roofer::reconstruction {

#define ROOFER_PLANE_DETECTOR_FIELDS(X)                                        \
  X(int, normal_neighbour_count, 5, "Neighbours used to estimate normals.",    \
    config::greater_than(0), public_)                                          \
  X(int, plane_neighbour_count, 15, "Neighbours used to grow planes.",         \
    config::greater_than(0), public_)                                          \
  X(int, min_plane_points, 15, "Minimum points in a detected plane.",          \
    config::greater_than(2), public_)                                          \
  X(float, plane_epsilon, 0.3F, "Maximum plane fitting distance, in metres.",  \
    config::greater_than(0.0F), public_)                                       \
  X(float, plane_normal_threshold, 0.75F,                                      \
    "Minimum normal dot-product similarity within a plane (unitless).",        \
    config::in_range(0.0F, 1.0F), public_)                                     \
  X(float, horizontal_threshold, 0.995F,                                       \
    "Minimum absolute normal/vertical dot product for horizontal planes "      \
    "(unitless).",                                                             \
    config::in_range(0.0F, 1.0F), public_)                                     \
  X(float, ransac_probability, 0.05F, "RANSAC miss probability.",              \
    config::in_range(0.0F, 1.0F), internal)                                    \
  X(float, ransac_cluster_epsilon, 0.3F,                                       \
    "RANSAC cluster distance, in metres.", config::greater_than(0.0F),         \
    internal)                                                                  \
  X(float, wall_threshold, 0.3F,                                               \
    "Maximum absolute normal/vertical dot product for wall planes "            \
    "(unitless).",                                                             \
    config::in_range(0.0F, 1.0F), public_)                                     \
  X(int, refit_interval, 5, "Plane refit interval.", config::greater_than(0),  \
    public_)                                                                   \
  X(bool, use_ransac, false, "Use RANSAC plane detection.",                    \
    config::no_validation<bool>(), internal)                                   \
  X(float, maximum_angle, 25.0F,                                               \
    "Maximum plane regularisation angle, in degrees.", config::at_least(0.0F), \
    public_)                                                                   \
  X(float, maximum_offset, 0.5F,                                               \
    "Maximum plane regularisation offset, in metres.", config::at_least(0.0F), \
    public_)                                                                   \
  X(bool, regularise_parallelism, false, "Regularise parallel planes.",        \
    config::no_validation<bool>(), public_)                                    \
  X(bool, regularise_orthogonality, false, "Regularise orthogonal planes.",    \
    config::no_validation<bool>(), public_)                                    \
  X(bool, regularise_coplanarity, false, "Regularise coplanar planes.",        \
    config::no_validation<bool>(), public_)                                    \
  X(bool, regularise_axis_symmetry, false, "Regularise plane symmetry.",       \
    config::no_validation<bool>(), public_)                                    \
  X(int, max_plane_count, 900, "Maximum detected planes before aborting.",     \
    config::greater_than(0), public_)

  struct PlaneDetectorConfig {
    using Self = PlaneDetectorConfig;
    ROOFER_CONFIG_MEMBERS(ROOFER_PLANE_DETECTOR_FIELDS)
  };
#undef ROOFER_PLANE_DETECTOR_FIELDS

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
