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
// Ivan Paden, Ravi Peters

#include <roofer/reconstruction/ElevationProvider.hpp>
#include <roofer/reconstruction/cdt_util.hpp>

#include <optional>

namespace roofer::reconstruction {

  namespace {
    using Polygon_2 = CGAL::Polygon_2<EPECK>;
    using Polygon_with_holes_2 = CGAL::Polygon_with_holes_2<EPECK>;

    bool is_inside(const Point_2& point, const Polygon_with_holes_2& polygon) {
      if (polygon.outer_boundary().bounded_side(point) !=
          CGAL::ON_BOUNDED_SIDE) {
        return false;
      }
      for (auto hole = polygon.holes_begin(); hole != polygon.holes_end();
           ++hole) {
        if (hole->bounded_side(point) != CGAL::ON_UNBOUNDED_SIDE) {
          return false;
        }
      }
      return true;
    }

    template <typename Curve>
    void add_boundary_intersections(const Curve& curve,
                                    const Polygon_2& boundary,
                                    std::vector<Point_2>& points) {
      for (auto edge = boundary.edges_begin(); edge != boundary.edges_end();
           ++edge) {
        auto result = CGAL::intersection(curve, *edge);
        if (!result) continue;
        if (const auto* point = std::get_if<Point_2>(&*result)) {
          points.push_back(*point);
        }
      }
    }

    template <typename Curve>
    void add_boundary_intersections(const Curve& curve,
                                    const Polygon_with_holes_2& polygon,
                                    std::vector<Point_2>& points) {
      add_boundary_intersections(curve, polygon.outer_boundary(), points);
      for (auto hole = polygon.holes_begin(); hole != polygon.holes_end();
           ++hole) {
        add_boundary_intersections(curve, *hole, points);
      }
    }

    std::vector<Segment_2> clip_line(const EPECK::Line_2& line,
                                     const Polygon_with_holes_2& polygon) {
      std::vector<Point_2> points;
      add_boundary_intersections(line, polygon, points);

      auto direction = line.to_vector();
      std::sort(
          points.begin(), points.end(),
          [&](const Point_2& lhs, const Point_2& rhs) {
            auto lhs_t = lhs.x() * direction.x() + lhs.y() * direction.y();
            auto rhs_t = rhs.x() * direction.x() + rhs.y() * direction.y();
            return lhs_t < rhs_t;
          });

      std::vector<Segment_2> result;
      for (size_t i = 1; i < points.size(); ++i) {
        if (points[i - 1] == points[i]) continue;
        Segment_2 segment(points[i - 1], points[i]);
        if (is_inside(CGAL::midpoint(segment.source(), segment.target()),
                      polygon)) {
          result.push_back(segment);
        }
      }
      return result;
    }

    std::vector<Segment_2> clip_segment(const Segment_2& segment,
                                        const Polygon_with_holes_2& polygon) {
      std::vector<Point_2> points = {segment.source(), segment.target()};
      add_boundary_intersections(segment, polygon, points);
      std::sort(points.begin(), points.end(),
                [&](const Point_2& lhs, const Point_2& rhs) {
                  return CGAL::squared_distance(segment.source(), lhs) <
                         CGAL::squared_distance(segment.source(), rhs);
                });

      std::vector<Segment_2> result;
      for (size_t i = 1; i < points.size(); ++i) {
        if (points[i - 1] == points[i]) continue;
        Segment_2 clipped(points[i - 1], points[i]);
        if (is_inside(CGAL::midpoint(clipped.source(), clipped.target()),
                      polygon)) {
          result.push_back(clipped);
        }
      }
      return result;
    }

    std::optional<Segment_2> triangle_intersection(
        const Plane& roof_plane, const proj_tri_util::DT::Triangle& triangle) {
      auto intersection = CGAL::intersection(roof_plane, triangle);
      if (!intersection) return std::nullopt;

      const auto* segment = std::get_if<EPICK::Segment_3>(&*intersection);
      if (!segment) return std::nullopt;

      return Segment_2(Point_2(segment->source().x(), segment->source().y()),
                       Point_2(segment->target().x(), segment->target().y()));
    }
  }  // namespace

  struct ConstantElevationProvider : public ElevationProvider {
    const float floor_elevation_;

    ConstantElevationProvider(const float floor_elevation)
        : floor_elevation_(floor_elevation){};

    virtual float get(const Point_2 /* pt */) const override {
      return floor_elevation_;
    }

    virtual float get_percentile(float /* percentile */) const override {
      return floor_elevation_;
    }

    std::vector<Segment_2> get_intersections(
        const Plane& roof_plane,
        const CGAL::Polygon_with_holes_2<EPECK>& bounds) const override {
      constexpr double epsilon = 1e-8;
      // check if plane is approximately horizontal
      if (std::abs(roof_plane.a()) < epsilon &&
          std::abs(roof_plane.b()) < epsilon) {
        return {};
      }
      EPECK::Line_2 intersection(
          roof_plane.a(), roof_plane.b(),
          roof_plane.c() * floor_elevation_ + roof_plane.d());
      return clip_line(intersection, bounds);
    }
  };

  struct InterpolatedElevationProvider : public ElevationProvider {
    std::shared_ptr<const proj_tri_util::DT> base_cdt_ptr_;

    InterpolatedElevationProvider(const proj_tri_util::DT& base_cdt)
        : base_cdt_ptr_(std::make_shared<const proj_tri_util::DT>(base_cdt)){};

    virtual float get(const Point_2 pt) const override {
      return proj_tri_util::interpolate_from_cdt(pt, *base_cdt_ptr_);
    }

    virtual float get_percentile(float percentile) const override {
      std::vector<float> elevations;
      // insert vertex elevation into the sorted vector
      auto insertSorted = [&elevations](float elevation) {
        auto position =
            std::lower_bound(elevations.begin(), elevations.end(), elevation);
        elevations.insert(position, elevation);
      };
      // iterate over all vertices and insert elevation
      for (auto& pt : base_cdt_ptr_->points()) insertSorted(pt.z());
      // return percentile
      return compute_percentile(elevations, percentile);
    }

    std::vector<Segment_2> get_intersections(
        const Plane& roof_plane,
        const CGAL::Polygon_with_holes_2<EPECK>& bounds) const override {
      std::vector<Segment_2> result;
      for (auto face = base_cdt_ptr_->finite_faces_begin();
           face != base_cdt_ptr_->finite_faces_end(); ++face) {
        auto intersection =
            triangle_intersection(roof_plane, base_cdt_ptr_->triangle(face));
        if (!intersection) continue;
        auto clipped = clip_segment(*intersection, bounds);
        result.insert(result.end(), clipped.begin(), clipped.end());
      }
      return result;
    }

   private:
    float compute_percentile(std::vector<float>& z_vec,
                             float percentile) const {
      assert(percentile <= 1.);
      assert(percentile >= 0.);
      size_t n = (z_vec.size() - 1) * percentile;
      std::nth_element(z_vec.begin(), z_vec.begin() + n, z_vec.end());
      return z_vec[n];
    }
  };

  std::unique_ptr<ElevationProvider> createElevationProvider(
      const float floor_elevation) {
    return std::make_unique<ConstantElevationProvider>(floor_elevation);
  }

  std::unique_ptr<ElevationProvider> createElevationProvider(
      const proj_tri_util::DT& base_cdt) {
    return std::make_unique<InterpolatedElevationProvider>(base_cdt);
  }

}  // namespace roofer::reconstruction
