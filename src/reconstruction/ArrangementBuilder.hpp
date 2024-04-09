#include "../datastructures.hpp"

#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct ArrangementBuilderConfig{
    int max_arr_complexity = 400;
    int dist_threshold_exp = 4;
    float fp_extension = 0.0;
    bool insert_with_snap = false;
    bool insert_lines = true;
  };

  struct ArrangementBuilderInterface {

    // add_vector_input("lines", {typeid(Segment), typeid(linereg::Segment_2)});
    // add_input("footprint", {typeid(linereg::Polygon_with_holes_2), typeid(LinearRing)});

    virtual ~ArrangementBuilderInterface() = default;
    virtual void compute(
      Arrangement_2& arrangement,
      LinearRing& footprint,
      std::vector<EPECK::Segment_2>& input_edges,
      ArrangementBuilderConfig config=ArrangementBuilderConfig()
    ) = 0;
    
  };

  std::unique_ptr<ArrangementBuilderInterface> createArrangementBuilder();
}