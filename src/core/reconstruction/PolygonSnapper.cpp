#include <roofer/reconstruction/PolygonSnapper.hpp>
#include <roofer/reconstruction/ArrangementBase.hpp>
#include "PolygonSnapping.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace roofer::reconstruction {
  namespace {
    using Ring = std::vector<Point_2>;
    // Even-odd membership works on raw crossing rings too. Noding is only an
    // intermediate representation: unresolved crossings are rejected below.
    bool inside(const Ring& ring, const Point_2& point) {
      bool result = false;
      for (std::size_t i = 0; i < ring.size(); ++i) {
        const auto& a = ring[i];
        const auto& b = ring[(i + 1) % ring.size()];
        if ((a.y() > point.y()) != (b.y() > point.y())) {
          auto x =
              a.x() + (point.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y());
          if (point.x() < x) result = !result;
        }
      }
      return result;
    }
  }  // namespace

  PolygonSnapResult snap_polygon(const PolygonRings& polygon,
                                 double threshold) {
    if (!std::isfinite(threshold) || threshold < 0)
      throw rooferException(
          "polygon snap threshold must be finite and nonnegative");
    if (threshold == 0) return {{polygon}, std::nullopt};
    PolygonSnapResult result;
    try {
      if (polygon.exterior.size() < 3)
        throw rooferException("polygon exterior has fewer than three vertices");
      const auto origin = polygon.exterior.front();
      std::vector<Ring> rings;
      std::vector<const std::vector<arr3d>*> source{&polygon.exterior};
      for (const auto& hole : polygon.holes) source.push_back(&hole);
      Arrangement_2 arrangement;
      for (const auto* ring : source) {
        auto& local = rings.emplace_back();
        for (const auto& point : *ring) {
          for (double coordinate : point)
            if (!std::isfinite(coordinate))
              throw rooferException("non-finite polygon coordinate");
          Point_2 p(point[0] - origin[0], point[1] - origin[1]);
          if (local.empty() || p != local.back()) local.push_back(p);
        }
        if (local.size() > 1 && local.front() == local.back()) local.pop_back();
        if (local.size() < 3)
          throw rooferException(
              "polygon ring has fewer than three distinct vertices");
        for (std::size_t i = 0; i < local.size(); ++i) {
          auto a = local[i], b = local[(i + 1) % local.size()];
          if (a != b) CGAL::insert(arrangement, Segment_2(a, b));
        }
      }
      CGAL::Arr_naive_point_location<Arrangement_2> location(arrangement);
      for (auto f : arrangement.face_handles()) {
        if (f->is_unbounded()) continue;
        auto point = arr_face_interior_point(f, location);
        if (!point) throw rooferException("unable to locate polygon interior");
        bool covered = inside(rings.front(), *point);
        bool hole = false;
        for (std::size_t i = 1; i < rings.size(); ++i)
          hole = hole || inside(rings[i], *point);
        if (hole && !covered)
          throw rooferException("polygon hole lies outside its exterior");
        f->data().in_footprint = covered && !hole;
        f->data().segid = f->data().in_footprint ? 1 : 0;
        f->data().plane = Plane(0, 0, 1, f->data().in_footprint ? -1 : 0);
      }
      if (!std::isfinite(float(threshold)) || float(threshold) == 0)
        throw rooferException(
            "polygon snap threshold is outside the supported range");
      std::map<Point_2, double> original_elevations;
      for (const auto* ring : source)
        for (const auto& p : *ring)
          original_elevations.emplace(
              Point_2(p[0] - origin[0], p[1] - origin[1]), p[2]);
      auto lift = [&](const Point_2& p) {
        if (auto original = original_elevations.find(p);
            original != original_elevations.end())
          return arr3d{CGAL::to_double(p.x()) + origin[0],
                       CGAL::to_double(p.y()) + origin[1], original->second};
        double best = std::numeric_limits<double>::infinity(), z = origin[2];
        for (std::size_t r = 0; r < source.size(); ++r) {
          const auto& original = *source[r];
          for (std::size_t i = 0; i < original.size(); ++i) {
            const auto& a = original[i];
            const auto& b = original[(i + 1) % original.size()];
            double ax = a[0] - origin[0], ay = a[1] - origin[1],
                   dx = b[0] - a[0], dy = b[1] - a[1];
            double x = CGAL::to_double(p.x()), y = CGAL::to_double(p.y());
            double length = dx * dx + dy * dy;
            double t =
                length == 0
                    ? 0
                    : std::clamp(((x - ax) * dx + (y - ay) * dy) / length, 0.0,
                                 1.0);
            double distance = (x - ax - t * dx) * (x - ax - t * dx) +
                              (y - ay - t * dy) * (y - ay - t * dy);
            if (distance < best) {
              best = distance;
              z = a[2] + t * (b[2] - a[2]);
            }
          }
        }
        return arr3d{CGAL::to_double(p.x()) + origin[0],
                     CGAL::to_double(p.y()) + origin[1], z};
      };
      result.polygons =
          detail::snapped_polygon_boundaries(arrangement, threshold, lift);
      if (result.polygons.empty())
        throw rooferException("polygon collapsed at the snap threshold");
    } catch (const std::exception& error) {
      result.polygons.clear();
      result.error = error.what();
    }
    return result;
  }
}  // namespace roofer::reconstruction
