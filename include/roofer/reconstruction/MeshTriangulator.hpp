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

#include "cgal_shared_definitions.hpp"

namespace roofer::reconstruction {

  struct MeshTriangulatorConfig {
    int dupe_threshold_exp = 4;
    bool output_all_triangles = false;
    bool output_mtc_for_every_input = false;
  };

  struct MeshTriangulatorInterface {
    TriangleCollection triangles;
    MultiTriangleCollection multitrianglecol;
    vec3f normals;
    vec1i ring_ids;
    vec1f volumes;

    virtual ~MeshTriangulatorInterface() = default;
    virtual void compute(
        const std::vector<Mesh>& meshes,
        MeshTriangulatorConfig config = MeshTriangulatorConfig()) = 0;
    virtual void compute(
        const std::vector<LinearRing>& polygons,
        MeshTriangulatorConfig config = MeshTriangulatorConfig()) = 0;
    virtual void compute(
        const std::unordered_map<int, Mesh>& multisolid,
        MeshTriangulatorConfig config = MeshTriangulatorConfig()) = 0;
  };

  std::unique_ptr<MeshTriangulatorInterface> createMeshTriangulatorLegacy();
  //   std::unique_ptr<MeshTriangulatorInterface>
  //   createMeshTriangulatorCGALPolyhedron();
}  // namespace roofer::reconstruction
