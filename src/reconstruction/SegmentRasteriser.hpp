#include "../datastructures.hpp"
#include "../datastructures/Raster.hpp"
#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct SegmentRasteriserConfig{
    float cellsize = 0.05;
    float thres_alpha = 0.25;
    bool use_ground = true;
    int megapixel_limit = 600;
    bool fill_nodata_ = true;
    int fill_nodata_window_size_ = 5;
  };

  struct SegmentRasteriserInterface {
    RasterTools::Raster heightfield;

      // add_vector_input("triangles", typeid(TriangleCollection));
      // add_vector_input("ground_triangles", typeid(TriangleCollection));
      // add_vector_input("alpha_rings", typeid(LinearRing));
      // add_input("roofplane_ids", typeid(vec1i));
      // add_input("pts_per_roofplane", typeid(IndexedPlanesWithPoints));
      // add_output("heightfield", typeid(RasterTools::Raster));
      
      // add_output("heightfield", typeid(RasterTools::Raster));
      // add_output("grid_points", typeid(PointCollection));
      // add_output("values", typeid(vec1f));
      // add_output("data_area", typeid(float));

    virtual ~SegmentRasteriserInterface() = default;
    virtual void compute(
      TriangleCollection& roof_triangles,
      TriangleCollection& ground_triangles,
      SegmentRasteriserConfig config=SegmentRasteriserConfig()
    ) = 0;
    
  };

  std::unique_ptr<SegmentRasteriserInterface> createSegmentRasteriser();
}