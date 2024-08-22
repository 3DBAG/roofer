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

namespace roofer::misc {

  struct PC2MeshDistCalculatorConfig {};

  struct PC2MeshDistCalculatorInterface {
    float rms_error;
    vec1f point_errors;
    vec1f face_errors;
    vec1f mesh_error;

    // add_output("error_hist", typeid(std::string));
    // add_output("m2pc_error_hist", typeid(std::string));
    // add_output("m2pc_error_max", typeid(float));

    virtual ~PC2MeshDistCalculatorInterface() = default;
    virtual void compute(
        const IndexedPlanesWithPoints& points,
        const MultiTriangleCollection& triangles, const roofer::vec1i& face_ids,
        PC2MeshDistCalculatorConfig config = PC2MeshDistCalculatorConfig()) = 0;
    virtual void compute(
        const PointCollection& points, const MultiTriangleCollection& triangles,
        const roofer::vec1i& face_ids,
        PC2MeshDistCalculatorConfig config = PC2MeshDistCalculatorConfig()) = 0;
  };

  std::unique_ptr<PC2MeshDistCalculatorInterface> createPC2MeshDistCalculator();
}  // namespace roofer::misc
