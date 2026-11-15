#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <roofer/common/datastructures.hpp>
#include <map>
#include <optional>
#include <set>
#include <type_traits>
#include <vector>

namespace roofer::reconstruction::detail {
  template <typename Coordinate>
  struct BoundaryPolygon {
    int label;
    std::vector<Coordinate> exterior;
    std::vector<std::vector<Coordinate>> holes;
  };

  // Read-only extraction from labelled triangle regions. Negative labels and
  // infinite faces are exterior. Keep the selected region on the left of each
  // directed edge, skipping internal edges through triangle adjacency.
  template <typename Tri, typename Label, typename Lift>
  auto triangulation_polygons(Tri& tri, Label label, Lift lift,
                              bool reject_point_contacts = false) {
    using Vertex = typename Tri::Vertex_handle;
    using Face = typename Tri::Face_handle;
    using Edge = std::pair<Face, int>;
    using Coordinate = std::invoke_result_t<Lift, Vertex>;
    using Kernel = CGAL::Exact_predicates_exact_constructions_kernel;
    using Polygon = CGAL::Polygon_2<Kernel>;
    struct Ring {
      int label;
      Polygon xy;
      std::vector<Vertex> vertices;
    };
    std::vector<BoundaryPolygon<Coordinate>> result;
    if (tri.dimension() < 2) return result;
    auto region = [&](Face f) { return tri.is_infinite(f) ? -1 : label(f); };
    std::set<Edge> boundary;
    std::map<std::pair<int, Vertex>, int> outgoing;
    for (auto f : tri.finite_face_handles()) {
      const int id = region(f);
      if (id < 0) continue;
      for (int i = 0; i < 3; ++i)
        if (region(f->neighbor(i)) != id) {
          boundary.emplace(f, i);
          if (++outgoing[{id, f->vertex(tri.ccw(i))}] > 1 &&
              reject_point_contacts)
            throw rooferException(
                "unresolved polygon crossing or point contact after snapping");
        }
    }
    std::set<Edge> visited;
    std::vector<Ring> exteriors, holes;
    for (auto f : tri.finite_face_handles())
      for (int i = 0; i < 3; ++i) {
        Edge start{f, i};
        if (!boundary.contains(start) || visited.contains(start)) continue;
        Ring ring{region(f), {}, {}};
        auto edge = start;
        do {
          if (!visited.insert(edge).second)
            throw rooferException(
                "triangulation boundary does not form a closed ring");
          auto v = edge.first->vertex(tri.ccw(edge.second));
          ring.vertices.push_back(v);
          Face next = edge.first;
          int index = tri.ccw(edge.second);
          std::size_t steps = 0;
          while (region(next->neighbor(index)) == ring.label) {
            auto previous = next;
            next = next->neighbor(index);
            index = tri.ccw(next->index(previous));
            if (++steps > tri.number_of_faces())
              throw rooferException(
                  "unable to find next triangulation boundary edge");
          }
          edge = {next, index};
          if (!boundary.contains(edge))
            throw rooferException("inconsistent triangulation boundary labels");
        } while (edge != start);
        // A triangle region can touch itself at a vertex. Decompose its
        // closed boundary walk into simple cycles, rather than treating the
        // repeated vertex as a self-intersection or following it indefinitely.
        std::vector<Vertex> path;
        std::map<Vertex, std::size_t> positions;
        for (std::size_t k = 0; k <= ring.vertices.size(); ++k) {
          auto vertex = ring.vertices[k % ring.vertices.size()];
          auto found = positions.find(vertex);
          if (found == positions.end()) {
            positions.emplace(vertex, path.size());
            path.push_back(vertex);
            continue;
          }
          const auto first = found->second;
          Ring cycle{ring.label, {}, {path.begin() + first, path.end()}};
          for (auto v : cycle.vertices)
            cycle.xy.push_back(
                typename Kernel::Point_2(CGAL::to_double(v->point().x()),
                                         CGAL::to_double(v->point().y())));
          if (cycle.xy.size() >= 3 && cycle.xy.area() != 0) {
            if (!cycle.xy.is_simple())
              throw rooferException("invalid triangulation boundary ring");
            (cycle.xy.is_counterclockwise_oriented() ? exteriors : holes)
                .push_back(std::move(cycle));
          }
          for (std::size_t j = first + 1; j < path.size(); ++j)
            positions.erase(path[j]);
          path.resize(first + 1);
        }
      }
    auto coordinates = [&](const Ring& ring) {
      std::vector<Coordinate> points;
      for (auto v : ring.vertices) points.push_back(lift(v));
      return points;
    };
    for (const auto& ring : exteriors)
      result.push_back({ring.label, coordinates(ring), {}});
    for (const auto& hole : holes) {
      std::optional<std::size_t> parent;
      for (std::size_t i = 0; i < exteriors.size(); ++i)
        if (exteriors[i].label == hole.label &&
            (!parent ||
             exteriors[i].xy.area() < exteriors[*parent].xy.area())) {
          // The first vertex may be a shared exterior/hole contact. A strict
          // interior boundary sample identifies the parent in that case too.
          bool contains = false;
          for (auto edge : hole.xy.edges())
            if (exteriors[i].xy.bounded_side(edge.source()) ==
                    CGAL::ON_BOUNDED_SIDE ||
                exteriors[i].xy.bounded_side(CGAL::midpoint(
                    edge.source(), edge.target())) == CGAL::ON_BOUNDED_SIDE) {
              contains = true;
              break;
            }
          if (contains) parent = i;
        }
      if (!parent)
        throw rooferException("unable to assign triangulation boundary hole");
      result[*parent].holes.push_back(coordinates(hole));
    }
    return result;
  }
}  // namespace roofer::reconstruction::detail
