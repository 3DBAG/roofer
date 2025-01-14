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

  struct PlaneIntersectorConfig {
    int min_neighb_pts = 5;
    float min_dist_to_line = 1.0;
    float min_length = 0;
    float thres_horiontality = 5;
  };

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
