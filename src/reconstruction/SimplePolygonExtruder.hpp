#include "../datastructures.hpp"

#include <memory>

namespace roofer::detection {

  struct SimplePolygonExtruderConfig{
  };

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
      LinearRing& footprint,
      float& floor_elevation,
      float& roof_elevation,
      SimplePolygonExtruderConfig config=SimplePolygonExtruderConfig()
    ) = 0;
    
  };

  std::unique_ptr<SimplePolygonExtruderInterface> createSimplePolygonExtruder();
}


