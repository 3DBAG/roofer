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
#include <roofer/reconstruction/cgal_shared_definitions.hpp>

namespace roofer::reconstruction {

  struct ArrangementBuilderConfig {
    int max_arr_complexity = 400;
    int dist_threshold_exp = 2;
    float fp_extension = 0.0;
    bool insert_with_snap = false;
    bool insert_lines = true;
  };

  struct ArrangementBuilderInterface {
    // add_vector_input("lines", {typeid(Segment), typeid(linereg::Segment_2)});
    // add_input("footprint", {typeid(linereg::Polygon_with_holes_2),
    // typeid(LinearRing)});

    virtual ~ArrangementBuilderInterface() = default;
    virtual void compute(
        Arrangement_2& arrangement, LinearRing& footprint,
        std::vector<EPECK::Segment_2>& input_edges,
        ArrangementBuilderConfig config = ArrangementBuilderConfig()) = 0;
  };

  std::unique_ptr<ArrangementBuilderInterface> createArrangementBuilder();
}  // namespace roofer::reconstruction
