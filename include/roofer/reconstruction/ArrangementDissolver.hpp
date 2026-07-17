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
#include <roofer/common/Raster.hpp>
#include <roofer/common/datastructures.hpp>
#include <roofer/reconstruction/ElevationProvider.hpp>
#include <roofer/reconstruction/cgal_shared_definitions.hpp>
#include <roofer/common/ConfigField.hpp>

namespace roofer::reconstruction {

#define ROOFER_ARRANGEMENT_DISSOLVER_FIELDS(X)                                \
  X(bool, dissolve_segment_edges, true, "Dissolve redundant segment edges.",  \
    config::no_validation<bool>(), internal)                                  \
  X(bool, dissolve_outside_footprint, true,                                   \
    "Dissolve outside-footprint faces.", config::no_validation<bool>(),       \
    internal)                                                                 \
  X(bool, dissolve_step_edges, false, "Dissolve height steps (LoD-derived).", \
    config::no_validation<bool>(), internal)                                  \
  X(bool, dissolve_all_interior, false,                                       \
    "Dissolve all interior edges (LoD-derived).",                             \
    config::no_validation<bool>(), internal)                                  \
  X(float, step_height_threshold, 3.0F,                                       \
    "Step-height threshold in metres (LoD-derived).",                         \
    config::greater_than(0.0F), internal)
  struct ArrangementDissolverConfig {
    using Self = ArrangementDissolverConfig;
    ROOFER_CONFIG_MEMBERS(ROOFER_ARRANGEMENT_DISSOLVER_FIELDS)
  };
#undef ROOFER_ARRANGEMENT_DISSOLVER_FIELDS

  struct ArrangementDissolverInterface {
    // add_input("arrangement", typeid(Arrangement_2));
    // add_input("heightfield", typeid(RasterTools::Raster));

    // add_output("global_elevation_97p", typeid(float));
    // add_output("global_elevation_70p", typeid(float));
    // add_output("global_elevation_50p", typeid(float));
    // add_output("global_elevation_min", typeid(float));
    // add_output("global_elevation_max", typeid(float));

    // add_output("arrangement", typeid(Arrangement_2));

    virtual ~ArrangementDissolverInterface() = default;
    virtual void compute(
        Arrangement_2& arrangement, const RasterTools::Raster& heightfield,
        const ElevationProvider& elevation_provider,
        ArrangementDissolverConfig config = ArrangementDissolverConfig()) = 0;

    virtual void compute(
        Arrangement_2& arrangement, const RasterTools::Raster& heightfield,
        float base_elevation,
        ArrangementDissolverConfig config = ArrangementDissolverConfig()) = 0;
  };

  std::unique_ptr<ArrangementDissolverInterface> createArrangementDissolver();
}  // namespace roofer::reconstruction
