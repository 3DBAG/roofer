#include "../datastructures.hpp"
#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct LineDetectorConfig{
    float dist_thres = 0.4;
    std::pair<int,int> min_cnt_range = {5,10};
    int min_cnt_range_lower = 5;
    int min_cnt_range_upper = 10;
    int k = 10;
    float snap_threshold = 1;
    float line_extend = 0.05;
    bool perform_chaining = true;
    bool remove_overlap = true;
  };

  struct LineDetectorInterface {
    SegmentCollection edge_segments;

    // add_vector_input("edge_points", {typeid(LinearRing)});
    // add_input("roofplane_ids", typeid(vec1i));
    // add_input("pts_per_roofplane", typeid(IndexedPlanesWithPoints ));
    // add_output("lines3d", typeid(SegmentCollection));
    // add_output("ring_edges", typeid(SegmentCollection));
    // add_output("ring_idx", typeid(std::unordered_map<size_t,std::vector<size_t>>));
    // add_output("ring_id", typeid(vec1i));
    // add_output("ring_order", typeid(vec1i));
    // add_output("is_start", typeid(vec1i));

    virtual ~LineDetectorInterface() = default;
    virtual void detect(
      const std::vector<LinearRing>& edge_points,
      const vec1i& roofplane_ids,
      const IndexedPlanesWithPoints& pts_per_roofplane,
      LineDetectorConfig config=LineDetectorConfig()
    ) = 0;
    
  };

  std::unique_ptr<LineDetectorInterface> createLineDetector();
}