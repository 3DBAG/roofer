#pragma once

#include <CGAL/centroid.h>
#include <CGAL/squared_distance_2.h>
#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace roofer::reconstruction::detail {
  // Finds what to snap. Adapters retain their own labels: collapse restores
  // constraints, reroute replaces a constrained edge by the two sides through
  // the opposite vertex.
  template <typename Tri, typename Collapse, typename Reroute, typename Cleanup>
  void snap_triangulation(Tri& tri, double squared_threshold, Collapse collapse,
                          Reroute reroute, Cleanup cleanup) {
    using Vertex = typename Tri::Vertex_handle;
    using Point = typename Tri::Point_2;
    bool changed;
    do {
      changed = false;
      for (auto f = tri.finite_faces_begin(); f != tri.finite_faces_end();
           ++f) {
        auto a = f->vertex(0), b = f->vertex(1), c = f->vertex(2);
        if (CGAL::squared_distance(a->point(), b->point()) <
                squared_threshold &&
            CGAL::squared_distance(b->point(), c->point()) <
                squared_threshold &&
            CGAL::squared_distance(c->point(), a->point()) <
                squared_threshold) {
          bool boundary = a->info() || b->info() || c->info();
          Point point =
              a->info()   ? a->point()
              : b->info() ? b->point()
              : c->info() ? c->point()
                          : CGAL::centroid(a->point(), b->point(), c->point());
          collapse(std::vector<Vertex>{a, b, c}, point, boundary);
          changed = true;
          break;
        }
      }
    } while (changed);
    do {
      changed = false;
      bool found = false;
      Vertex first, second;
      double distance = 0;
      auto less = [](const Point& a, const Point& b) {
        return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
      };
      for (auto e = tri.finite_edges_begin(); e != tri.finite_edges_end();
           ++e) {
        auto a = e->first->vertex(tri.cw(e->second)),
             b = e->first->vertex(tri.ccw(e->second));
        if (less(b->point(), a->point())) std::swap(a, b);
        double d =
            CGAL::to_double(CGAL::squared_distance(a->point(), b->point()));
        if (d >= squared_threshold) continue;
        if (!found || d < distance ||
            (d == distance && (less(a->point(), first->point()) ||
                               (a->point() == first->point() &&
                                less(b->point(), second->point()))))) {
          first = a;
          second = b;
          distance = d;
          found = true;
        }
      }
      if (found) {
        bool boundary = first->info() || second->info();
        Point point = first->info() ? first->point()
                      : second->info()
                          ? second->point()
                          : CGAL::midpoint(first->point(), second->point());
        collapse(std::vector<Vertex>{first, second}, point, boundary);
        changed = true;
      }
    } while (changed);
    // Faces go stale after a change, so rescan. New constraints are shorter
    // than the removed one, so this ends. A shared side represents every
    // original constraint routed along it. Their vertex handles stay valid
    // throughout this phase.
    using Segment = typename Tri::Segment;
    using EdgeKey = std::pair<Vertex, Vertex>;
    std::map<EdgeKey, std::set<EdgeKey>> origins;
    auto key = [](Vertex a, Vertex b) {
      return a < b ? std::pair(a, b) : std::pair(b, a);
    };
    auto origins_for = [&](const EdgeKey& edge, bool was_constrained) -> auto& {
      auto& sources = origins[edge];
      if (sources.empty() && was_constrained) sources.insert(edge);
      return sources;
    };
    do {
      changed = false;
      for (auto f = tri.finite_faces_begin(); f != tri.finite_faces_end();
           ++f) {
        for (int i = 0; i < 3; ++i) {
          auto a = f->vertex(tri.cw(i)), b = f->vertex(tri.ccw(i)),
               opposite = f->vertex(i);
          if (!tri.is_constrained({f, i})) continue;
          auto length = CGAL::squared_distance(a->point(), b->point());
          bool a_side = tri.is_constrained({f, tri.ccw(i)}),
               b_side = tri.is_constrained({f, tri.cw(i)});
          if ((a_side || b_side) &&
              CGAL::squared_distance(tri.segment({f, i}), opposite->point()) <
                  squared_threshold &&
              CGAL::squared_distance(a->point(), opposite->point()) < length &&
              CGAL::squared_distance(b->point(), opposite->point()) < length) {
            const auto edge = key(a, b);
            const auto& sources = origins_for(edge, true);
            auto near_opposite = [&](const EdgeKey& source) {
              Segment segment(source.first->point(), source.second->point());
              return CGAL::squared_distance(segment, opposite->point()) <
                     squared_threshold;
            };
            if (!std::all_of(sources.begin(), sources.end(), near_opposite))
              continue;
            reroute(typename Tri::Edge(f, i), opposite);
            for (auto [endpoint, was_constrained] :
                 {std::pair(a, a_side), std::pair(b, b_side)}) {
              auto& side =
                  origins_for(key(endpoint, opposite), was_constrained);
              side.insert(sources.begin(), sources.end());
            }
            origins.erase(edge);
            changed = true;
            break;
          }
        }
        if (changed) break;
      }
    } while (changed);
    cleanup();
  }
}  // namespace roofer::reconstruction::detail
