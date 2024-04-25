#include "SimplePolygonExtruder.hpp"

#include <algorithm>

namespace roofer::detection {
  
  class SimplePolygonExtruder : public SimplePolygonExtruderInterface{


    void compute(
      LinearRing& ring,
      float& h_floor,
      float& h_roof,
      SimplePolygonExtruderConfig cfg
    ) override {
      // assume ring is CCW oriented
      Mesh mesh;

      //floor
      LinearRing r_floor = ring;
      for (auto& p : r_floor) p[2] = h_floor;
      for (auto& hole_floor : r_floor.interior_rings()) {
        for (auto& p : hole_floor) p[2] = h_floor;
      }

      //roof
      LinearRing r_roof = ring;
      for (auto& p : r_roof) p[2] = h_roof;
      for (auto& hole_floor : r_roof.interior_rings()) {
        for (auto& p : hole_floor) p[2] = h_roof;
      }
      polygons_3d.push_back(r_roof);
      surface_types.push_back(2);
      mesh.push_polygon(r_roof, int(1));
      // mesh.push_attribute("surface_type", int(2));

      //walls
      size_t j_prev = ring.size()-1;
      for (size_t j=0; j<ring.size(); ++j) {
        LinearRing wall;
        wall.push_back(r_floor[j_prev]);
        wall.push_back(r_floor[j]);
        wall.push_back(r_roof[j]);
        wall.push_back(r_roof[j_prev]);

        polygons_3d.push_back(wall);
        surface_types.push_back(1);
        mesh.push_polygon(wall, int(2));

        // mesh.push_attribute("surface_type", int(1));
        j_prev=j;
      }
      // walls from holes
      for (auto& hole : r_floor.interior_rings()) {
        size_t j_prev = hole.size()-1;
        for (size_t j=0; j<hole.size(); ++j) {
          LinearRing wall;
          wall.push_back(hole[j_prev]);
          wall.push_back(hole[j]);
          wall.push_back(arr3f{hole[j][0], hole[j][1], h_roof});
          wall.push_back(arr3f{hole[j_prev][0], hole[j_prev][1], h_roof});

          polygons_3d.push_back(wall);
          surface_types.push_back(1);
          mesh.push_polygon(wall, int(2));
          j_prev=j;
        }
      }
      
      //floor
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


  std::unique_ptr<SimplePolygonExtruderInterface> createSimplePolygonExtruder() {
    return std::make_unique<SimplePolygonExtruder>();
  }
}