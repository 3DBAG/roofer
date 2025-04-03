// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ravi Peters

#include <map>
#include <roofer/reconstruction/PlaneIntersector.hpp>

namespace roofer::reconstruction {

  class PlaneIntersector : public PlaneIntersectorInterface {
    bool get_line_extend(EPICK::Line_3* l, const std::vector<Point>& points,
                         double& dmin, double& dmax, Point& pmin, Point& pmax,
                         float& min_dist_to_line_sq) {
      size_t cnt = 0;
      bool setminmax = false;
      double sqd_min = 1 + min_dist_to_line_sq;

      auto lp = l->point();
      auto lv = l->to_vector();
      lv = lv / CGAL::sqrt(lv.squared_length());

      for (const auto& p : points) {
        auto sqd = CGAL::squared_distance(*l, p);
        if (sqd > min_dist_to_line_sq) continue;
        auto av = p - lp;
        auto d = av * lv;
        if (!setminmax) {
          setminmax = true;
          dmin = dmax = d;
          pmin = pmax = p;
        }
        if (d < dmin) {
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
    void compute(const IndexedPlanesWithPoints& pts_per_roofplane,
                 const std::map<size_t, std::map<size_t, size_t>>& plane_adj,
                 PlaneIntersectorConfig cfg) override {
      float min_dist_to_line_sq = cfg.min_dist_to_line * cfg.min_dist_to_line;
      float sq_min_length = cfg.min_length * cfg.min_length;

      vec1f length;
      vec1f dists;
      size_t ring_cntr = 0;
      for (auto& [id_hi, ids_lo] : plane_adj) {
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
            if (auto l = std::get_if<typename EPICK::Line_3>(&*result)) {
              Point pmin_lo, pmax_lo;
              Point pmin_hi, pmax_hi;
              double dmin_lo, dmax_lo;
              double dmin_hi, dmax_hi;

              // skip this line if it is too far away any of the plane_pts
              if (!get_line_extend(l, plane_pts_hi, dmin_hi, dmax_hi, pmin_hi,
                                   pmax_hi, min_dist_to_line_sq) ||
                  !get_line_extend(l, plane_pts_lo, dmin_lo, dmax_lo, pmin_lo,
                                   pmax_lo, min_dist_to_line_sq))
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
              if (!((std::isinf(sx) || std::isinf(sy)) ||
                    (std::isinf(tx) || std::isinf(ty)))) {
                sq_length = float(
                    CGAL::to_double(CGAL::squared_distance(ppmin, ppmax)));
                if (sq_length > 1E-10) {
                  arr3f source = {sx, sy, float(CGAL::to_double(ppmin.z()))};
                  arr3f target = {tx, ty, float(CGAL::to_double(ppmax.z()))};
                  if (sq_length > sq_min_length) {
                    segments.push_back({source, target});
                    length.push_back(std::sqrt(sq_length));
                    // check if the line is a ridgeline by checking if plane_hi
                    // and plane_lo have a normal that has an angle of more than
                    // thres_horiontality degrees with the positive z axis
                    auto n_hi = plane_hi.orthogonal_vector();
                    // ensure that n_hi is pointing upwards
                    if (float(CGAL::to_double(n_hi.z())) < 0) {
                      n_hi = -n_hi;
                    }
                    auto n_lo = plane_lo.orthogonal_vector();
                    // ensure that n_lo is pointing upwards
                    if (float(CGAL::to_double(n_lo.z())) < 0) {
                      n_lo = -n_lo;
                    }
                    auto n_z = EPICK::Vector_3(0, 0, 1);
                    bool is_ridge = CGAL::approximate_angle(n_z, n_hi) >
                                        cfg.thres_horiontality &&
                                    CGAL::approximate_angle(n_z, n_lo) >
                                        cfg.thres_horiontality;
                    auto a = CGAL::approximate_angle(ppmin, ppmax, ppmax + n_z);
                    bool is_horizontal = a < (90 + cfg.thres_horiontality) &&
                                         a > (90 - cfg.thres_horiontality);
                    if (is_ridge && is_horizontal) {
                      is_ridgeline.push_back(true);
                    } else {
                      is_ridgeline.push_back(false);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    size_t find_highest_ridgeline(float& high_z, size_t& high_i) override {
      size_t i_max, n = 0;
      float z_max = -std::numeric_limits<float>::max();
      for (size_t i = 0; i < segments.size(); ++i) {
        if (is_ridgeline[i]) {
          if (segments[i][0][2] > z_max) {
            z_max = segments[i][0][2];
            i_max = i;
          } else if (segments[i][1][2] > z_max) {
            z_max = segments[i][1][2];
            i_max = i;
          }
          ++n;
        };
      }
      if (z_max == -std::numeric_limits<float>::max()) return 0;
      high_i = i_max;
      high_z = z_max;
      return n;
    }
  };

  std::unique_ptr<PlaneIntersectorInterface> createPlaneIntersector() {
    return std::make_unique<PlaneIntersector>();
  };

}  // namespace roofer::reconstruction
