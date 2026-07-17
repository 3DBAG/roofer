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

  using PointCountRange = std::pair<int, int>;
#define ROOFER_LINE_DETECTOR_FIELDS(X)                                       \
  X(float, distance_threshold, 1.0F,                                         \
    "Maximum line fitting distance, in metres.", config::greater_than(0.0F), \
    public_)                                                                 \
  X(PointCountRange, min_point_count_range, (PointCountRange{5, 10}),        \
    "Minimum point-count range.", config::ordered_range(1), public_)         \
  X(int, neighbour_count, 10, "Neighbours used for line detection.",         \
    config::greater_than(0), public_)                                        \
  X(float, snap_threshold, 1.0F,                                             \
    "Line endpoint snapping distance, in metres.", config::at_least(0.0F),   \
    public_)                                                                 \
  X(float, extension, 0.05F, "Line extension distance, in metres.",          \
    config::at_least(0.0F), public_)                                         \
  X(bool, perform_chaining, true, "Chain connected lines.",                  \
    config::no_validation<bool>(), public_)                                  \
  X(bool, remove_overlap, true, "Remove overlapping lines.",                 \
    config::no_validation<bool>(), public_)
  struct LineDetectorConfig {
    using Self = LineDetectorConfig;
    ROOFER_CONFIG_MEMBERS(ROOFER_LINE_DETECTOR_FIELDS)
  };
#undef ROOFER_LINE_DETECTOR_FIELDS

  struct LineDetectorInterface {
    SegmentCollection edge_segments;

    // add_vector_input("edge_points", {typeid(LinearRing)});
    // add_input("roofplane_ids", typeid(vec1i));
    // add_input("pts_per_roofplane", typeid(IndexedPlanesWithPoints ));
    // add_output("lines3d", typeid(SegmentCollection));
    // add_output("ring_edges", typeid(SegmentCollection));
    // add_output("ring_idx",
    // typeid(std::unordered_map<size_t,std::vector<size_t>>));
    // add_output("ring_id", typeid(vec1i));
    // add_output("ring_order", typeid(vec1i));
    // add_output("is_start", typeid(vec1i));

    virtual ~LineDetectorInterface() = default;
    virtual void detect(const std::vector<LinearRing>& edge_points,
                        const vec1i& roofplane_ids,
                        const IndexedPlanesWithPoints& pts_per_roofplane,
                        LineDetectorConfig config = LineDetectorConfig()) = 0;
  };

  std::unique_ptr<LineDetectorInterface> createLineDetector();
}  // namespace roofer::reconstruction
