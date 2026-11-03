#include "WallAssembler.hpp"

#include <CGAL/squared_distance_3.h>
#include <algorithm>
#include <map>
#include <set>

namespace roofer::reconstruction::detail {
  namespace {
    arr3f lifted(const Point_2& p, float z) {
      return {float(CGAL::to_double(p.x())), float(CGAL::to_double(p.y())), z};
    }

    void append(vec3f& ring, arr3f p) {
      if (ring.empty() || ring.back() != p) ring.push_back(p);
    }

    template <typename Callback>
    void visit_rings(Mesh& mesh, Callback&& visit) {
      for (auto& polygon : mesh.get_polygons()) {
        visit(polygon);
        for (auto& hole : polygon.interior_rings()) visit(hole);
      }
    }
  }  // namespace

  void WallAssembler::add(const Point_2& p, const Point_2& q,
                          std::array<float, 2> lower,
                          std::array<float, 2> upper, int part, int label) {
    if (p != q && lower != upper)
      spans_.push_back({p, q, lower, upper, part, label});
  }

  void WallAssembler::emit(std::unordered_map<int, Mesh>& meshes) const {
    const auto count = spans_.size();
    std::map<Point_2, std::vector<std::size_t>> starts;
    for (std::size_t i = 0; i < count; ++i) starts[spans_[i].p].push_back(i);
    std::vector<std::size_t> next(count, count), previous(count, count);
    std::vector<int> outgoing(count), incoming(count);
    for (std::size_t i = 0; i < count; ++i) {
      const auto& a = spans_[i];
      auto candidates = starts.find(a.q);
      if (candidates == starts.end()) continue;
      for (auto j : candidates->second) {
        const auto& b = spans_[j];
        if (a.part != b.part || a.label != b.label ||
            !CGAL::collinear(a.p, a.q, b.q) ||
            CGAL::scalar_product(a.q - a.p, b.q - b.p) <= 0 ||
            std::max(a.lower[1], b.lower[0]) >=
                std::min(a.upper[1], b.upper[0]))
          continue;
        next[i] = j;
        previous[j] = i;
        ++outgoing[i];
        ++incoming[j];
      }
    }
    // Join only unambiguous continuations. A positive vertical overlap above
    // excludes disconnected spans and point-only contacts.
    for (std::size_t i = 0; i < count; ++i) {
      if (outgoing[i] != 1 || incoming[next[i]] != 1) next[i] = count;
      if (incoming[i] != 1 || outgoing[previous[i]] != 1) previous[i] = count;
    }
    for (std::size_t i = 0; i < count; ++i) {
      if (previous[i] != count) continue;
      LinearRing wall;
      vec3f top;
      // Every continuation advances along one oriented line, so chains cannot
      // cycle. Height steps remain in both boundary profiles.
      for (auto j = i; j != count; j = next[j]) {
        const auto& span = spans_[j];
        append(wall, lifted(span.p, span.lower[0]));
        append(wall, lifted(span.q, span.lower[1]));
        append(top, lifted(span.p, span.upper[0]));
        append(top, lifted(span.q, span.upper[1]));
      }
      for (auto point = top.rbegin(); point != top.rend(); ++point)
        append(wall, *point);
      if (wall.size() > 1 && wall.front() == wall.back()) wall.pop_back();
      if (wall.size() > 2)
        meshes[spans_[i].part].push_polygon(wall, spans_[i].label);
    }
  }

  // Split vertical edges at shared incident heights to remove T-junctions.
  void conform_vertical_edges(std::unordered_map<int, Mesh>& meshes) {
    for (auto& [part, mesh] : meshes) {
      std::map<arr2f, std::set<float>> columns;
      visit_rings(mesh, [&](const vec3f& ring) {
        for (const auto& p : ring) columns[{p[0], p[1]}].insert(p[2]);
      });
      visit_rings(mesh, [&](vec3f& ring) {
        vec3f result;
        for (std::size_t i = 0; i < ring.size(); ++i) {
          const auto p = ring[i], q = ring[(i + 1) % ring.size()];
          append(result, p);
          if (p[0] != q[0] || p[1] != q[1] || p[2] == q[2]) continue;
          vec3f extra;
          for (auto z : columns.at({p[0], p[1]}))
            if (z > std::min(p[2], q[2]) && z < std::max(p[2], q[2]))
              extra.push_back({p[0], p[1], z});
          if (p[2] > q[2]) std::reverse(extra.begin(), extra.end());
          result.insert(result.end(), extra.begin(), extra.end());
        }
        ring = std::move(result);
      });
    }
  }

  void remove_redundant_vertices(std::unordered_map<int, Mesh>& meshes,
                                 float tolerance) {
    for (auto& [part, mesh] : meshes) {
      std::map<arr3f, std::set<arr3f>> neighbours;
      visit_rings(mesh, [&](const vec3f& ring) {
        for (std::size_t i = 0; i < ring.size(); ++i) {
          const auto& p = ring[i];
          const auto& q = ring[(i + 1) % ring.size()];
          neighbours[p].insert(q);
          neighbours[q].insert(p);
        }
      });
      std::set<arr3f> redundant;
      auto exact = [](const arr3f& p) {
        return EPECK::Point_3(p[0], p[1], p[2]);
      };
      const EPECK::FT tolerance_sq = double(tolerance) * tolerance;
      auto simplify = [&](const std::vector<arr3f>& chain, std::size_t first,
                          std::size_t last) {
        for (auto i = first + 1; i < last; ++i) redundant.insert(chain[i]);
        std::vector<std::pair<std::size_t, std::size_t>> pending{{first, last}};
        while (!pending.empty()) {
          auto [a, b] = pending.back();
          pending.pop_back();
          const EPECK::Segment_3 segment(exact(chain[a]), exact(chain[b]));
          auto maximum = tolerance_sq;
          auto split = b;
          for (auto i = a + 1; i < b; ++i) {
            auto distance = CGAL::squared_distance(exact(chain[i]), segment);
            if (distance > maximum) {
              maximum = distance;
              split = i;
            }
          }
          if (split != b) {
            redundant.erase(chain[split]);
            pending.emplace_back(a, split);
            pending.emplace_back(split, b);
          }
        }
      };
      std::set<arr3f> visited;
      auto trace = [&](const arr3f& start, arr3f next) {
        std::vector<arr3f> chain{start};
        auto previous = start;
        for (;;) {
          chain.push_back(next);
          const auto& adjacent = neighbours.at(next);
          if (next == start || adjacent.size() != 2) break;
          visited.insert(next);
          auto other = *adjacent.begin() == previous ? *adjacent.rbegin()
                                                     : *adjacent.begin();
          previous = next;
          next = other;
        }
        const auto last = chain.size() - 1;
        if (chain.front() != chain.back()) {
          simplify(chain, 0, last);
        } else {
          // A degree-two cycle has no junction anchors. Split it into two
          // arcs at the vertex farthest from its start before simplifying.
          std::size_t opposite = 1;
          for (std::size_t i = 2; i < last; ++i)
            if (CGAL::squared_distance(exact(start), exact(chain[i])) >
                CGAL::squared_distance(exact(start), exact(chain[opposite])))
              opposite = i;
          simplify(chain, 0, opposite);
          simplify(chain, opposite, last);
        }
      };
      // Junctions anchor degree-two chains. Bound deviation against the entire
      // original chain, rather than independently deleting almost straight
      // vertices: small local errors must not accumulate into a large change.
      for (const auto& [p, adjacent] : neighbours)
        if (adjacent.size() != 2)
          for (const auto& next : adjacent)
            if (!visited.contains(next)) trace(p, next);
      for (const auto& [p, adjacent] : neighbours)
        if (adjacent.size() == 2 && !visited.contains(p)) {
          visited.insert(p);
          trace(p, *adjacent.begin());
        }
      // Preserve valid rings even if presented with degenerate input geometry.
      visit_rings(mesh, [&](const vec3f& ring) {
        if (std::count_if(ring.begin(), ring.end(), [&](const auto& p) {
              return !redundant.contains(p);
            }) < 3)
          for (const auto& p : ring) redundant.erase(p);
      });
      visit_rings(mesh, [&](vec3f& ring) {
        std::erase_if(ring,
                      [&](const auto& p) { return redundant.contains(p); });
      });
    }
  }
}  // namespace roofer::reconstruction::detail
