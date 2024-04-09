#include "../datastructures.hpp"

#include "../datastructures/Raster.hpp"
#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct ArrangementOptimiserConfig{
    float data_multiplier = 8.0;
    float smoothness_multiplier = 1.0;
    bool preset_labels = false;
    bool do_normalise = false;
    int n_iterations = 1;
    int graph_cut_impl = 0;
    bool use_ground = true;
    bool label_ground_outside_fp = true;
    float z_percentile = 0.9;
  };

  struct ArrangementOptimiserInterface {
    
      // add_input("arrangement", typeid(Arrangement_2));
      // add_input("heightfield", typeid(RasterTools::Raster));
      // add_input("pts_per_roofplane", typeid(IndexedPlanesWithPoints ));
      // add_input("ground_pts_per_roofplane", typeid(IndexedPlanesWithPoints ));
      // add_output("arrangement", typeid(Arrangement_2));
      // add_vector_output("groundparts", typeid(LinearRing));

    virtual ~ArrangementOptimiserInterface() = default;
    virtual void compute(
      Arrangement_2& arrangement,
      const RasterTools::Raster& heightfield,
      const IndexedPlanesWithPoints& roof_planes,
      const IndexedPlanesWithPoints& ground_planes,
      ArrangementOptimiserConfig config=ArrangementOptimiserConfig()
    ) = 0;
    
  };

  std::vector<LinearRing> arr2polygons(Arrangement_2& arr);

  std::unique_ptr<ArrangementOptimiserInterface> createArrangementOptimiser();
}