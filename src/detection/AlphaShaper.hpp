#include "../datastructures.hpp"
#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct AlphaShaperConfig{
    float thres_alpha = 0.25;
    bool extract_polygons = true;
    bool optimal_alpha = true;
    bool optimal_only_if_needed = true;
  };

  struct AlphaShaperInterface {
    std::vector<LinearRing> alpha_rings;
    TriangleCollection alpha_triangles;
    vec1i roofplane_ids;

    // add_output("edge_points", typeid(PointCollection));
    // add_output("alpha_edges", typeid(LineStringCollection));
    // add_output("segment_ids", typeid(vec1i));

    virtual ~AlphaShaperInterface() = default;
    virtual void compute(const IndexedPlanesWithPoints& pts_per_roofplane, AlphaShaperConfig config=AlphaShaperConfig()) = 0;
  };

  std::unique_ptr<AlphaShaperInterface> createAlphaShaper();
}