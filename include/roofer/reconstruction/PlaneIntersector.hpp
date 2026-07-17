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

#define ROOFER_PLANE_INTERSECTOR_FIELDS(X)                               \
  X(int, min_neighbour_points, 5,                                        \
    "Minimum number of neighbouring plane points.", config::at_least(0), \
    public_)                                                             \
  X(float, distance_to_line_threshold, 1.0F,                             \
    "Maximum distance from plane points to the intersection line, in "   \
    "metres.",                                                           \
    config::at_least(0.0F), public_)                                     \
  X(float, min_length, 0.0F, "Minimum intersection length, in metres.",  \
    config::at_least(0.0F), public_)                                     \
  X(float, horizontality_threshold, 5.0F,                                \
    "Intersection horizontality threshold for ridgeline detection, in "  \
    "degrees.",                                                          \
    config::at_least(0.0F), public_)
  struct PlaneIntersectorConfig {
    using Self = PlaneIntersectorConfig;
    ROOFER_CONFIG_MEMBERS(ROOFER_PLANE_INTERSECTOR_FIELDS)
  };
#undef ROOFER_PLANE_INTERSECTOR_FIELDS

  struct PlaneIntersectorInterface {
    SegmentCollection segments;
    vec1b is_ridgeline;

    virtual ~PlaneIntersectorInterface() = default;
    virtual void compute(
        const IndexedPlanesWithPoints& pts_per_roofplane,
        const std::map<size_t, std::map<size_t, size_t>>& plane_adj,
        PlaneIntersectorConfig config = PlaneIntersectorConfig()) = 0;

    // find highest ridgeline
    virtual size_t find_highest_ridgeline(float& high_z, size_t& high_i) = 0;
  };

  std::unique_ptr<PlaneIntersectorInterface> createPlaneIntersector();
}  // namespace roofer::reconstruction
