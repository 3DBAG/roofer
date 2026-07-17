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
#include <roofer/reconstruction/cgal_shared_definitions.hpp>
#include <roofer/common/ConfigField.hpp>

namespace roofer::reconstruction {

#define ROOFER_ARRANGEMENT_BUILDER_FIELDS(X)                                   \
  X(int, snap_tolerance_exp, 2,                                                \
    "Base-10 exponent for the snap tolerance in metres.", config::at_least(0), \
    internal)                                                                  \
  X(float, footprint_extension, 0.0F,                                          \
    "Footprint extension distance, in metres.", config::at_least(0.0F),        \
    public_)                                                                   \
  X(bool, insert_with_snap, false, "Snap while inserting arrangement edges.",  \
    config::no_validation<bool>(), internal)                                   \
  X(bool, insert_lines, true, "Insert detected lines.",                        \
    config::no_validation<bool>(), internal)
  struct ArrangementBuilderConfig {
    using Self = ArrangementBuilderConfig;
    ROOFER_CONFIG_MEMBERS(ROOFER_ARRANGEMENT_BUILDER_FIELDS)
  };
#undef ROOFER_ARRANGEMENT_BUILDER_FIELDS

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
