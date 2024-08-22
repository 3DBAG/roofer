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

namespace roofer::reconstruction {

  struct SimplePolygonExtruderConfig {};

  struct SimplePolygonExtruderInterface {
    std::vector<LinearRing> polygons_3d;
    vec1i surface_types;
    std::unordered_map<int, Mesh> multisolid;

    // add_input("polygon", typeid(LinearRing));
    // add_input("floor_elevation", typeid(float));
    // add_input("roof_elevation", typeid(float));
    // add_vector_output("3d_polygons", typeid(LinearRing));
    // add_output("surface_types", typeid(vec1i));

    virtual ~SimplePolygonExtruderInterface() = default;
    virtual void compute(
        LinearRing& footprint, float& floor_elevation, float& roof_elevation,
        SimplePolygonExtruderConfig config = SimplePolygonExtruderConfig()) = 0;
  };

  std::unique_ptr<SimplePolygonExtruderInterface> createSimplePolygonExtruder();
}  // namespace roofer::reconstruction
