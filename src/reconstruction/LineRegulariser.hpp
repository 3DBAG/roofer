#include "../datastructures.hpp"
#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct LineRegulariserConfig{
    float dist_threshold = 0.5;
    float angle_threshold = 0.15;
    float extension = 1.0;
    bool merge_intersection_lines = false;
  };

  struct LineRegulariserInterface {
    std::vector<EPECK::Segment_2> exact_regularised_edges;
    std::vector<Segment> regularised_edges;

    // add_input("edge_segments", {typeid(SegmentCollection), typeid(LineString)});
    // add_input("ints_segments", typeid(SegmentCollection));

    // add_vector_output("regularised_edges", typeid(Segment));
    // add_vector_output("exact_regularised_edges", typeid());
    // add_output("edges_out_", typeid(SegmentCollection));
    // add_output("priorities", typeid(vec1i));
    // add_output("angle_cluster_id", typeid(vec1i));
    // add_output("dist_cluster_id", typeid(vec1i));
    // add_output("exact_footprint_out", typeid(linereg::Polygon_with_holes_2));
    // add_output("n_angle_clusters", typeid(int));

    virtual ~LineRegulariserInterface() = default;
    virtual void compute(
      const SegmentCollection& edge_segments,
      const SegmentCollection& ints_segments,
      LineRegulariserConfig config=LineRegulariserConfig()
    ) = 0;
    
  };

  std::unique_ptr<LineRegulariserInterface> createLineRegulariser();
}