#include "PlaneIntersector.hpp"

#include <map>

namespace roofer::detection {
  
  class PlaneIntersector : public PlaneIntersectorInterface {

    bool get_line_extend(
      EPICK::Line_3* l,
      const std::vector<Point>& points,
      double& dmin,
      double& dmax,
      Point& pmin,
      Point& pmax,
      float& min_dist_to_line_sq
    ) {
      size_t cnt = 0;
      bool setminmax=false;  
      double sqd_min = 1 + min_dist_to_line_sq;

      auto lp = l->point();
      auto lv = l->to_vector();
      lv = lv / CGAL::sqrt(lv.squared_length());

      for (const auto& p : points) {
        auto sqd = CGAL::squared_distance(*l, p);
        if (sqd > min_dist_to_line_sq)
          continue;
        auto av = p-lp;
        auto d = av*lv;
        if (!setminmax) {
          setminmax=true;
          dmin=dmax=d;
          pmin=pmax=p;
        }
        if (d < dmin){
          dmin = d;
          pmin = p;
        }
        if (d > dmax) {
          dmax = d;
          pmax = p;
        }
        sqd_min = std::min(sqd_min, sqd);
        ++cnt;
      }
      return cnt > 1;
    }

    public:
    void compute(
        const IndexedPlanesWithPoints& pts_per_roofplane,
        const std::map<size_t, std::map<size_t, size_t>>& plane_adj,
        PlaneIntersectorConfig cfg
      ) override {
      float min_dist_to_line_sq = cfg.min_dist_to_line*cfg.min_dist_to_line;
      float sq_min_length = cfg.min_length*cfg.min_length;

      vec1f length;
      vec1f dists;
      size_t ring_cntr=0;
      for(auto& [id_hi, ids_lo] : plane_adj) {
        auto& plane_hi = pts_per_roofplane.at(id_hi).first;
        auto& plane_pts_hi = pts_per_roofplane.at(id_hi).second;
        // // auto& alpha_ring = alpha_rings.get<LinearRing>(ring_cntr++);
        for (auto& [id_lo, cnt] : ids_lo) {
          // skip plain pairs with  low number of neighbouring points
          if (cnt < cfg.min_neighb_pts) continue;
          auto& plane_lo = pts_per_roofplane.at(id_lo).first;
          auto& plane_pts_lo = pts_per_roofplane.at(id_lo).second;
          auto result = CGAL::intersection(plane_hi, plane_lo);
          if (result) {
            // bound the line to extend of one plane's inliers
            if (auto l = boost::get<typename EPICK::Line_3>(&*result)) {
              Point pmin_lo, pmax_lo;
              Point pmin_hi, pmax_hi;
              double dmin_lo, dmax_lo;
              double dmin_hi, dmax_hi;
              
              // skip this line if it is too far away any of the plane_pts
              if( !get_line_extend(l, plane_pts_hi, dmin_hi, dmax_hi, pmin_hi, pmax_hi, min_dist_to_line_sq) || 
                  !get_line_extend(l, plane_pts_lo, dmin_lo, dmax_lo, pmin_lo, pmax_lo, min_dist_to_line_sq) )
                continue;

              // take the overlap between the two extends
              Point ppmin, ppmax;
              if (dmin_lo > dmin_hi)
                ppmin = l->projection(pmin_lo);
              else
                ppmin = l->projection(pmin_hi);
              
              if (dmax_lo < dmax_hi)
                ppmax = l->projection(pmax_lo);
              else
                ppmax = l->projection(pmax_hi);
              
              // Check for infinity (quick fix for linux crash)
              auto sx = float(CGAL::to_double(ppmin.x()));
              auto sy = float(CGAL::to_double(ppmin.y()));
              auto tx = float(CGAL::to_double(ppmax.x()));
              auto ty = float(CGAL::to_double(ppmax.y()));
              float sq_length;
              if (!((std::isinf(sx) || std::isinf(sy)) || (std::isinf(tx) || std::isinf(ty)))) {
                sq_length = float(CGAL::to_double(CGAL::squared_distance(ppmin, ppmax)));
                if(sq_length > 1E-10) {
                  arr3f source = {
                    sx,
                    sy,
                    float(CGAL::to_double(ppmin.z()))
                  };
                  arr3f target = {
                    tx,
                    ty,
                    float(CGAL::to_double(ppmax.z()))
                  };
                  if (sq_length > sq_min_length) {
                    segments.push_back({source,target});
                    length.push_back(std::sqrt(sq_length));
                  }
                }
              }
            }
          }
        }
      }
    }
  };

  std::unique_ptr<PlaneIntersectorInterface> createPlaneIntersector() {
    return std::make_unique<PlaneIntersector>();
  };

}