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

#include <CGAL/linear_least_squares_fitting_3.h>

#include <cassert>
#include <roofer/reconstruction/cgal_shared_definitions.hpp>
#include <roofer/misc/MeshPropertyCalculator.hpp>
#include <roofer/common/formatters.hpp>

namespace roofer::misc {

  typedef CGAL::Simple_cartesian<float> CF;
  static constexpr double pi = 3.14159265358979323846;
  static const CF::Vector_3 up = CF::Vector_3(0, 0, 1);

  struct MeshPropertyCalculator : public MeshPropertyCalculatorInterface {
    void rasterise_ring(LinearRing& polygon, RasterTools::Raster& r) {
      typedef double FT;
      typedef CGAL::Simple_cartesian<FT> K;

      K::Plane_3 plane;
      std::vector<K::Point_3> pts;
      for (auto& p : polygon) {
        pts.push_back(K::Point_3(p[0], p[1], p[2]));
      }
      // Fit a plane to the polygon vertices using CGAL's linear least squares fitting
      // The Dimension_tag<0> indicates we're fitting to a set of points (0-dimensional objects)
      // This computes the best-fit plane through the 3D points that minimizes squared distances
      linear_least_squares_fitting_3(pts.begin(), pts.end(), plane,
                                     CGAL::Dimension_tag<0>());

      auto box = polygon.box();
      auto bb_min = box.min();
      auto bb_max = box.max();
      auto cr_min = r.getColRowCoord(bb_min[0], bb_min[1]);
      auto cr_max = r.getColRowCoord(bb_max[0], bb_max[1]);

      auto points_inside = r.rasterise_polygon(polygon, cr_min, cr_max);
      for (auto& p : points_inside) {
        float z_interpolate = -plane.a() / plane.c() * p[0] -
                              plane.b() / plane.c() * p[1] -
                              plane.d() / plane.c();
        r.add_point(p[0], p[1], z_interpolate, RasterTools::MAX);
      }
    };

    void calculate_h_attr(Mesh& mesh, RasterTools::Raster& r_lod22,
                          ComputeRoofHeightConfig cfg) override {
      auto& faces = mesh.get_polygons();
      auto& labels = mesh.get_labels();
      auto& attributes = mesh.get_attributes();
      for (size_t i = 0; i < faces.size(); ++i) {
        if (labels[i] == 1) {
          auto& polygon = faces.at(i);
          auto box = polygon.box();
          auto bb_min = box.min();
          auto bb_max = box.max();
          auto cr_min = r_lod22.getColRowCoord(bb_min[0], bb_min[1]);
          auto cr_max = r_lod22.getColRowCoord(bb_max[0], bb_max[1]);
          auto part_points =
              r_lod22.rasterise_polygon(polygon, cr_min, cr_max, false);

          if (part_points.size() == 0) {
            part_points.insert(part_points.begin(), polygon.begin(),
                               polygon.end());
          }
          std::sort(part_points.begin(), part_points.end(),
                    [](auto& p1, auto& p2) { return p1[2] < p2[2]; });

          size_t N = part_points.size();
          int elevation_id = std::floor(0.5 * float(N - 1));
          attributes[i].insert(
              cfg.h_50p, float(part_points[elevation_id][2] + cfg.z_offset));
          elevation_id = std::floor(0.7 * float(N - 1));
          attributes[i].insert(
              cfg.h_70p, float(part_points[elevation_id][2] + cfg.z_offset));
          auto h_min = float(part_points[0][2] + cfg.z_offset);
          auto h_max = float(part_points[N - 1][2] + cfg.z_offset);
          attributes[i].insert(cfg.h_min, h_min);
          attributes[i].insert(cfg.h_max, h_max);
        }
      }
    };

    RasterTools::Raster get_heightmap(std::unordered_map<int, roofer::Mesh>& multisolid, float cellsize) override {
      Box box;

      for (auto& [i, mesh] : multisolid) {
        auto& faces = mesh.get_polygons();
        auto& labels = mesh.get_labels();
        assert(faces.size() == labels.size());
        size_t n_faces = faces.size();

        auto& attributes = mesh.get_attributes();
        assert(attributes.size() == n_faces);
        for (size_t i = 0; i < n_faces; ++i) {
            if (labels[i] == 1) {
                box.add(faces[i].box());
            }
        }
      }
      auto boxmin = box.min();
      auto boxmax = box.max();

      RasterTools::Raster r_lod22 =
          RasterTools::Raster(cellsize, boxmin[0] - 0.5, boxmax[0] + 0.5,
                              boxmin[1] - 0.5, boxmax[1] + 0.5);
      r_lod22.prefill_arrays(RasterTools::MAX);
      for (auto& [i, mesh] : multisolid) {
        auto& faces = mesh.get_polygons();
        auto& labels = mesh.get_labels();
        size_t n_faces = faces.size();
        for (size_t i = 0; i < n_faces; ++i) {
            if (labels[i] == 1) {
                rasterise_ring(faces[i], r_lod22);
            }
        }
      }
      return r_lod22;
    };

    CF::Vector_3 calculate_normal_cf(const LinearRing& ring) {
      float x = 0, y = 0, z = 0;
      for (size_t i = 0; i < ring.size(); ++i) {
        const auto& curr = ring[i];
        const auto& next = ring[(i + 1) % ring.size()];
        x += (curr[1] - next[1]) * (curr[2] + next[2]);
        y += (curr[2] - next[2]) * (curr[0] + next[0]);
        z += (curr[0] - next[0]) * (curr[1] + next[1]);
      }
      CF::Vector_3 n(x, y, z);
      return n / CGAL::approximate_sqrt(n.squared_length());
    };

    void compute_roof_orientation(Mesh& mesh,
                                  ComputeRoofOrientationsConfig cfg) override {
      auto& faces = mesh.get_polygons();
      auto& labels = mesh.get_labels();
      auto& attributes = mesh.get_attributes();
      assert(faces.size() == labels.size());
      size_t n_faces = faces.size();
      assert(attributes.size() == n_faces);

      // compute inclination and azimuth. Both in degrees.
      // size_t cnt_flat = 0, cnt_slant = 0;
      for (size_t i = 0; i < n_faces; ++i) {
        if (labels[i] == 1) {
          auto& ring = faces.at(i);
          auto n = calculate_normal_cf(ring);
          float azimuth, slope;
          if (std::isnan(n.x()) || std::isnan(n.y()) || std::isnan(n.z())) {
            azimuth = std::numeric_limits<float>::quiet_NaN();
            slope = std::numeric_limits<float>::quiet_NaN();
          } else {
            slope = CGAL::to_double(CGAL::approximate_angle(n, up));

            // if (slope < cfg.is_horizontal_threshold) {
            //   ++cnt_flat;
            // } else {
            //   ++cnt_slant;
            // }

            // calculate azimuth from arctan2
            // (https://en.cppreference.com/w/cpp/numeric/math/atan2) ie.
            // subtract pi/2, multiply by -1 and then add 2 pi if result is
            // negative (4th quadrant)
            azimuth = -1 * (std::atan2(n.y(), n.x()) - pi / 2);
            if (azimuth < 0) {
              azimuth = 2 * pi + azimuth;
            }

            // convert to degrees
            azimuth = azimuth * (180 / pi);
          }
          // push attributes
          attributes[i].insert(cfg.slope, slope);
          attributes[i].insert(cfg.azimuth, azimuth);

          // std::string roof_type = "could not detect";
          // if (cnt_slant)
          // attributes[i].insert(cfg.roof_type, azimuth);
        }
      }
    }
  };

  std::unique_ptr<MeshPropertyCalculatorInterface>
  createMeshPropertyCalculator() {
    return std::make_unique<MeshPropertyCalculator>();
  }
}  // namespace roofer::misc
