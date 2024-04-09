#include "../datastructures.hpp"
#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct PlaneIntersectorConfig{
    int min_neighb_pts = 5;
    float min_dist_to_line = 1.0;
    float min_length = 0;
  };

  struct PlaneIntersectorInterface {
    SegmentCollection segments;

    virtual ~PlaneIntersectorInterface() = default;
    virtual void compute(
      const IndexedPlanesWithPoints& pts_per_roofplane,
      const std::map<size_t, std::map<size_t, size_t>>& plane_adj,
      PlaneIntersectorConfig config=PlaneIntersectorConfig()
    ) = 0;
    
  };

  std::unique_ptr<PlaneIntersectorInterface> createPlaneIntersector();
}