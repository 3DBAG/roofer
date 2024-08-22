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

#include <algorithm>
#include <roofer/reconstruction/SimplePolygonExtruder.hpp>

namespace roofer::reconstruction {

  class SimplePolygonExtruder : public SimplePolygonExtruderInterface {
    void compute(LinearRing& ring, float& h_floor, float& h_roof,
                 SimplePolygonExtruderConfig cfg) override {
      // assume ring is CCW oriented
      Mesh mesh;

      // floor
      LinearRing r_floor = ring;
      for (auto& p : r_floor) p[2] = h_floor;
      for (auto& hole_floor : r_floor.interior_rings()) {
        for (auto& p : hole_floor) p[2] = h_floor;
      }

      // roof
      LinearRing r_roof = ring;
      for (auto& p : r_roof) p[2] = h_roof;
      for (auto& hole_floor : r_roof.interior_rings()) {
        for (auto& p : hole_floor) p[2] = h_roof;
      }
      polygons_3d.push_back(r_roof);
      surface_types.push_back(2);
      mesh.push_polygon(r_roof, int(1));
      // mesh.push_attribute("surface_type", int(2));

      // walls
      size_t j_prev = ring.size() - 1;
      for (size_t j = 0; j < ring.size(); ++j) {
        LinearRing wall;
        wall.push_back(r_floor[j_prev]);
        wall.push_back(r_floor[j]);
        wall.push_back(r_roof[j]);
        wall.push_back(r_roof[j_prev]);

        polygons_3d.push_back(wall);
        surface_types.push_back(1);
        mesh.push_polygon(wall, int(2));

        // mesh.push_attribute("surface_type", int(1));
        j_prev = j;
      }
      // walls from holes
      for (auto& hole : r_floor.interior_rings()) {
        size_t j_prev = hole.size() - 1;
        for (size_t j = 0; j < hole.size(); ++j) {
          LinearRing wall;
          wall.push_back(hole[j_prev]);
          wall.push_back(hole[j]);
          wall.push_back(arr3f{hole[j][0], hole[j][1], h_roof});
          wall.push_back(arr3f{hole[j_prev][0], hole[j_prev][1], h_roof});

          polygons_3d.push_back(wall);
          surface_types.push_back(1);
          mesh.push_polygon(wall, int(2));
          j_prev = j;
        }
      }

      // floor
      std::reverse(r_floor.begin(), r_floor.end());
      for (auto& hole : r_floor.interior_rings()) {
        std::reverse(hole.begin(), hole.end());
      }
      polygons_3d.push_back(r_floor);
      surface_types.push_back(0);
      mesh.push_polygon(r_floor, int(0));
      // mesh.push_attribute("surface_type", int(0));
      multisolid[0] = mesh;
    }
  };

  std::unique_ptr<SimplePolygonExtruderInterface>
  createSimplePolygonExtruder() {
    return std::make_unique<SimplePolygonExtruder>();
  }
}  // namespace roofer::reconstruction
