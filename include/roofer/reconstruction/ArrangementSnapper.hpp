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
#include <roofer/reconstruction/ElevationProvider.hpp>
#include <roofer/reconstruction/cgal_shared_definitions.hpp>
#include <roofer/common/ConfigField.hpp>

namespace roofer::reconstruction {

#define ROOFER_ARRANGEMENT_SNAPPER_FIELDS(X)                                   \
  X(float, distance_threshold, 0.021F, "Vertex snapping distance, in metres.", \
    config::at_least(0.0F), public_)                                           \
  X(bool, repair_non_manifold_vertices, true, "Repair non-manifold vertices.", \
    config::no_validation<bool>(), public_)                                    \
  X(float, manifold_repair_radius, 0.02F,                                      \
    "Non-manifold repair radius, in metres.", config::greater_than(0.0F),      \
    public_)                                                                   \
  X(float, manifold_height_tolerance, 1e-4F,                                   \
    "Non-manifold height tolerance, in metres.", config::at_least(0.0F),       \
    public_)
  struct ArrangementSnapperConfig {
    using Self = ArrangementSnapperConfig;
    ROOFER_CONFIG_MEMBERS(ROOFER_ARRANGEMENT_SNAPPER_FIELDS)
  };
#undef ROOFER_ARRANGEMENT_SNAPPER_FIELDS

  struct ArrangementSnapperInterface {
    // add_output("triangles_og", typeid(TriangleCollection));
    // add_output("segment_ids_og", typeid(vec1i));
    // add_output("triangles_snapped", typeid(TriangleCollection));
    // add_output("segment_ids_snapped", typeid(vec1i));

    virtual ~ArrangementSnapperInterface() = default;
    virtual void compute(
        Arrangement_2& arrangement,
        ArrangementSnapperConfig config = ArrangementSnapperConfig()) = 0;

    virtual void compute(
        Arrangement_2& arrangement, const ElevationProvider& elevation_provider,
        ArrangementSnapperConfig config = ArrangementSnapperConfig()) = 0;

    virtual void compute(
        Arrangement_2& arrangement, float base_elevation,
        ArrangementSnapperConfig config = ArrangementSnapperConfig()) = 0;
  };

  std::unique_ptr<ArrangementSnapperInterface> createArrangementSnapper();
}  // namespace roofer::reconstruction
