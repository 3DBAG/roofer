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
// Ivan Paden

#pragma once
#include <memory>
#include <roofer/common/datastructures.hpp>
#include <roofer/reconstruction/ElevationProvider.hpp>
#include <roofer/reconstruction/cgal_shared_definitions.hpp>
#include <roofer/common/ConfigField.hpp>
#include <vector>

namespace roofer::reconstruction {

#define ROOFER_ARRANGEMENT_EXTRUDER_FIELDS(X)                                  \
  X(bool, generate_walls, true, "Generate walls.",                             \
    config::no_validation<bool>(), public_)                                    \
  X(bool, generate_roofs, true, "Generate roofs.",                             \
    config::no_validation<bool>(), public_)                                    \
  X(bool, generate_floor, true, "Generate floors.",                            \
    config::no_validation<bool>(), public_)                                    \
  X(bool, lod1_extrude_to_max, false,                                          \
    "Extrude LoD 1 to maximum roof height"                                     \
    " instead of 70th percentile.",                                            \
    config::no_validation<bool>(), public_)                                    \
  X(float, nodata_elevation, 3.0F,                                             \
    "Fallback elevation for nodata faces, in metres.", config::at_least(0.0F), \
    internal)                                                                  \
  X(float, snap_tolerance_exp, 2.9F,                                           \
    "Base-10 exponent for the snap tolerance in metres.",                      \
    config::at_least(0.0F), public_)                                           \
  X(bool, lod2, true, "Generate LoD 2 topology (pipeline-derived).",           \
    config::no_validation<bool>(), internal)
  struct ArrangementExtruderConfig {
    using Self = ArrangementExtruderConfig;
    ROOFER_CONFIG_MEMBERS(ROOFER_ARRANGEMENT_EXTRUDER_FIELDS)
  };
#undef ROOFER_ARRANGEMENT_EXTRUDER_FIELDS

  struct ArrangementExtruderInterface {
    vec1i labels;
    std::vector<LinearRing> faces;
    std::vector<Mesh> meshes;
    std::unordered_map<int, Mesh> multisolid;
    // add_input("arrangement", typeid(Arrangement_2));
    // add_input("floor_elevation", typeid(float));

    // // add_output("normals_vec3f", typeid(vec3f), true);
    // add_vector_output("labels", typeid(int)); // 0==ground, 1==roof,
    // 2==outerwall, 3==innerwall add_vector_output("faces",
    // typeid(LinearRing)); add_vector_output("mesh", typeid(Mesh));
    // add_output("multisolid", typeid(std::unordered_map<int, Mesh>));

    virtual ~ArrangementExtruderInterface() = default;

    virtual void compute(
        Arrangement_2& arrangement, const ElevationProvider& elevation_provider,
        ArrangementExtruderConfig config = ArrangementExtruderConfig()) = 0;

    virtual void compute(
        Arrangement_2& arrangement, const float base_elevation,
        ArrangementExtruderConfig config = ArrangementExtruderConfig()) = 0;
  };

  std::unique_ptr<ArrangementExtruderInterface> createArrangementExtruder();
}  // namespace roofer::reconstruction
