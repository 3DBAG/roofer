#include "SegmentRasteriser.hpp"
// #include "spdlog/spdlog.h"

namespace roofer::detection {
  
  class SegmentRasteriser : public SegmentRasteriserInterface {

    void rasterise_input(const TriangleCollection& triangle_collection, RasterTools::Raster& r, size_t& data_pixel_cnt) {
      typedef CGAL::Exact_predicates_inexact_constructions_kernel K;     
      for (const auto& triangle : triangle_collection) {
        CGAL::Plane_3<K> plane(
          K::Point_3(triangle[0][0], triangle[0][1], triangle[0][2]),
          K::Point_3(triangle[1][0], triangle[1][1], triangle[1][2]),
          K::Point_3(triangle[2][0], triangle[2][1], triangle[2][2])
        );
        roofer::Box box;
        for (auto& p : triangle) {
          box.add(p);
        }
        auto bb_min = box.min();
        auto bb_max = box.max();
        auto cr_min = r.getColRowCoord(bb_min[0], bb_min[1]);
        auto cr_max = r.getColRowCoord(bb_max[0], bb_max[1]);

        auto points_inside = r.rasterise_polygon(triangle, cr_min, cr_max);
        for (auto& p : points_inside) {
          double z_interpolate = -plane.a()/plane.c() * p[0] - plane.b()/plane.c()*p[1] - plane.d()/plane.c();
          if (r.add_point(p[0], p[1], z_interpolate, RasterTools::MAX)) {
            ++data_pixel_cnt; //only count new cells (that were not written to before)
          }
        }
        // do plane projection
        // auto& plane = pts_per_roofplane[roofplane_ids[i]].first;
        // double z_interpolate = -plane.a()/plane.c() * p[0] - plane.b()/plane.c()*p[1] - plane.d()/plane.c();
        // r.add_point(p[0], p[1], z_interpolate, RasterTools::MAX);
      }
    }
    void compute(
      TriangleCollection& roof_triangles,
      TriangleCollection& ground_triangles,
      SegmentRasteriserConfig cfg
    ) override {

      // spdlog::debug("roof_triangles has {} triangles", roof_triangles.size());
      // spdlog::debug("ground_triangles has {} triangles", ground_triangles.size());

      Box box;
        box.add(roof_triangles.box());
      if(cfg.use_ground) {
        box.add(ground_triangles.box());
      }
      auto cellsize_ = cfg.cellsize;
      auto boxmin = box.min();
      auto boxmax = box.max();
      auto pixel_limit = cfg.megapixel_limit * 1E6;
      while(true) {
        auto dimx = (boxmax[0]-boxmin[0])/cellsize_ + 1;
        auto dimy = (boxmax[1]-boxmin[1])/cellsize_ + 1;
        if(dimx*dimy > pixel_limit) {
          cellsize_*=2;
        } else break;
      }
      heightfield = RasterTools::Raster(cellsize_, boxmin[0]-0.5, boxmax[0]+0.5, boxmin[1]-0.5, boxmax[1]+0.5);
      heightfield.prefill_arrays(RasterTools::MAX);

      size_t roofdata_area_cnt = 0, grounddata_area_cnt = 0;
      rasterise_input(roof_triangles, heightfield, roofdata_area_cnt);
      if(cfg.use_ground)
        rasterise_input(ground_triangles, heightfield, grounddata_area_cnt);

      if (cfg.fill_nodata_) heightfield.fill_nn(cfg.fill_nodata_window_size_);

      PointCollection grid_points;
      vec1f values;
      double nodata = heightfield.getNoDataVal();
      for(size_t i=0; i<heightfield.dimx_ ; ++i) {
        for(size_t j=0; j<heightfield.dimy_ ; ++j) {
          auto p = heightfield.getPointFromRasterCoords(i,j);
          if (p[2]!=nodata) {
            grid_points.push_back(p);
            values.push_back(p[2]);
          }
        }
      }
      // spdlog::debug("heightfield has {} values", values.size());
      // output("data_area").set(float(roofdata_area_cnt)*cellsize_*cellsize_);
      // output("heightfield").set(r);
      // output("values").set(values);
      // output("grid_points").set(grid_points);
    }
  };

  std::unique_ptr<SegmentRasteriserInterface> createSegmentRasteriser() {
    return std::make_unique<SegmentRasteriser>();
  };

}