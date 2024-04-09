#include "../datastructures.hpp"

#include "../datastructures/Raster.hpp"
#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct ArrangementDissolverConfig{
    bool dissolve_seg_edges = true;
    bool dissolve_step_edges = false;
    bool dissolve_outside_fp = true;
    bool dissolve_all_interior = false;
    bool skip_execution = false;
    float step_height_threshold = 3.0;
  };

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
      Arrangement_2& arrangement,
      const RasterTools::Raster& heightfield,
      ArrangementDissolverConfig config=ArrangementDissolverConfig()
    ) = 0;
    
  };

  std::unique_ptr<ArrangementDissolverInterface> createArrangementDissolver();
}