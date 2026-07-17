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

#define ROOFER_LINE_REGULARISER_FIELDS(X)                                      \
  X(float, distance_threshold, 0.8F, "Line merge distance, in metres.",        \
    config::at_least(0.0F), public_)                                           \
  X(float, angle_threshold, 8.594367F,                                         \
    "Line merge angle threshold, in degrees.", config::in_range(0.0F, 180.0F), \
    public_)                                                                   \
  X(float, extension, 3.0F, "Regularised line extension, in metres.",          \
    config::at_least(0.0F), public_)                                           \
  X(bool, merge_intersection_lines, false, "Merge plane intersection lines.",  \
    config::no_validation<bool>(), public_)
  struct LineRegulariserConfig {
    using Self = LineRegulariserConfig;
    ROOFER_CONFIG_MEMBERS(ROOFER_LINE_REGULARISER_FIELDS)
  };
#undef ROOFER_LINE_REGULARISER_FIELDS

  struct LineRegulariserInterface {
    std::vector<EPECK::Segment_2> exact_regularised_edges;
    std::vector<Segment> regularised_edges;

    // add_input("edge_segments", {typeid(SegmentCollection),
    // typeid(LineString)}); add_input("ints_segments",
    // typeid(SegmentCollection));

    // add_vector_output("regularised_edges", typeid(Segment));
    // add_vector_output("exact_regularised_edges", typeid());
    // add_output("edges_out_", typeid(SegmentCollection));
    // add_output("priorities", typeid(vec1i));
    // add_output("angle_cluster_id", typeid(vec1i));
    // add_output("dist_cluster_id", typeid(vec1i));
    // add_output("exact_footprint_out", typeid(linereg::Polygon_with_holes_2));
    // add_output("n_angle_clusters", typeid(int));

    virtual ~LineRegulariserInterface() = default;
    virtual void compute(
        const SegmentCollection& edge_segments,
        const SegmentCollection& ints_segments,
        LineRegulariserConfig config = LineRegulariserConfig()) = 0;
  };

  std::unique_ptr<LineRegulariserInterface> createLineRegulariser();
}  // namespace roofer::reconstruction
