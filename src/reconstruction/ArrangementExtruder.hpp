#include "../datastructures.hpp"

#include "cgal_shared_definitions.hpp"
#include "cdt_util.hpp"
#include <memory>
#include <vector>

namespace roofer::detection {

  struct ArrangementExtruderConfig{
    bool do_walls=true, do_roofs=true, do_floor=true;
    bool LoD2 = true;
    bool lod1_extrude_to_max_ = false;
    // float base_elevation = 0;
    float nodata_elevation = 3;
    int snap_tolerance_exp = 4;
  };

  struct ArrangementExtruderInterface {
    vec1i labels;
    std::vector<LinearRing> faces;
    std::vector<Mesh> meshes;
    std::unordered_map<int, Mesh> multisolid ;
  // add_input("arrangement", typeid(Arrangement_2));
  // add_input("floor_elevation", typeid(float));

  // // add_output("normals_vec3f", typeid(vec3f), true);
  // add_vector_output("labels", typeid(int)); // 0==ground, 1==roof, 2==outerwall, 3==innerwall
  // add_vector_output("faces", typeid(LinearRing));
  // add_vector_output("mesh", typeid(Mesh));
  // add_output("multisolid", typeid(std::unordered_map<int, Mesh>));

    virtual ~ArrangementExtruderInterface() = default;

    virtual void compute(
        Arrangement_2& arrangement,
        const float floor_elevation,
        ArrangementExtruderConfig config=ArrangementExtruderConfig(),
        std::unique_ptr<proj_tri_util::CDT> base_cdt = nullptr
    ) = 0;

  };

  std::unique_ptr<ArrangementExtruderInterface> createArrangementExtruder();
}