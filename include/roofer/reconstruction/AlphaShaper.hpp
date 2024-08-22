// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of geoflow-roofer (https://github.com/3DBAG/geoflow-roofer)

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
#include <roofer/reconstruction/cgal_shared_definitions.hpp>

namespace roofer::reconstruction {

  struct AlphaShaperConfig {
    float thres_alpha = 0.25;
    bool extract_polygons = true;
    bool optimal_alpha = true;
    bool optimal_only_if_needed = true;
  };

  struct AlphaShaperInterface {
    std::vector<LinearRing> alpha_rings;
    TriangleCollection alpha_triangles;
    vec1i roofplane_ids;

    // add_output("edge_points", typeid(PointCollection));
    // add_output("alpha_edges", typeid(LineStringCollection));
    // add_output("segment_ids", typeid(vec1i));

    virtual ~AlphaShaperInterface() = default;
    virtual void compute(const IndexedPlanesWithPoints& pts_per_roofplane,
                         AlphaShaperConfig config = AlphaShaperConfig()) = 0;
  };

  std::unique_ptr<AlphaShaperInterface> createAlphaShaper();
}  // namespace roofer::reconstruction
