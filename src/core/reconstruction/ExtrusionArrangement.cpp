#include "ExtrusionArrangement.hpp"
#include "ExtrusionUtils.hpp"

#include <CGAL/Arr_naive_point_location.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <map>
#include <set>

namespace roofer::reconstruction::detail {
  void VertexColumn::snap(float tolerance) {
    if (heights.empty()) return;
    std::sort(heights.begin(), heights.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    float reference = heights.front().first;
    for (auto& [height, face] : heights) {
      if (height - reference < tolerance) height = reference;
      reference = height;
    }
  }

  float VertexColumn::height(Face_const_handle face) const {
    for (const auto& [height, incident] : heights)
      if (incident == face) return height;
    throw rooferException("Missing roof vertex height");
  }

  std::optional<EPECK::Point_3> profile_crossing(const Point_2& p,
                                                 const Point_2& q,
                                                 const HeightProfile& a,
                                                 const HeightProfile& b) {
    if (!((a.source < b.source && a.target > b.target) ||
          (a.source > b.source && a.target < b.target)))
      return std::nullopt;
    const auto dp = a.source - b.source, dq = a.target - b.target;
    const auto t = dp / (dp - dq);
    const auto xy = p + (q - p) * t;
    return EPECK::Point_3(xy.x(), xy.y(), a.source + (a.target - a.source) * t);
  }

  namespace {
    using Polygon = Polygon_2;
    using Domain = Polygon_with_holes_2;
    using Location = CGAL::Arr_naive_point_location<Arrangement_2>;

    // Clean duplicate vertices and validate a ring using XY coordinates only.
    Polygon ring_polygon(const vec3f& ring) {
      Polygon result;
      for (const auto& p : ring) {
        if (!std::isfinite(p[0]) || !std::isfinite(p[1]))
          throw rooferException("non-finite underpass coordinate");
        Point_2 q(p[0], p[1]);
        if (result.is_empty() || result[result.size() - 1] != q)
          result.push_back(q);
      }
      if (result.size() > 1 && result[0] == result[result.size() - 1])
        result.erase(result.vertices_end() - 1);
      if (result.size() < 3 || !result.is_simple() || result.area() == 0)
        throw rooferException("invalid underpass polygon ring");
      return result;
    }

    // Test whether two polygon boundaries cross or touch.
    bool boundaries_intersect(const Polygon& a, const Polygon& b) {
      for (const auto& e : a.edges())
        for (const auto& f : b.edges())
          if (CGAL::do_intersect(e, f)) return true;
      return false;
    }

    // Build a valid polygon with a CCW exterior and CW pillar holes.
    Domain input_polygon(const LinearRing& input) {
      auto outer = ring_polygon(input);
      if (outer.is_clockwise_oriented()) outer.reverse_orientation();
      std::vector<Polygon> holes;
      for (const auto& ring : input.interior_rings()) {
        auto hole = ring_polygon(ring);
        if (outer.bounded_side(hole[0]) != CGAL::ON_BOUNDED_SIDE ||
            boundaries_intersect(outer, hole))
          throw rooferException(
              "underpass hole lies outside or touches its exterior");
        for (const auto& other : holes) {
          if (boundaries_intersect(hole, other) ||
              hole.bounded_side(other[0]) != CGAL::ON_UNBOUNDED_SIDE ||
              other.bounded_side(hole[0]) != CGAL::ON_UNBOUNDED_SIDE)
            throw rooferException("underpass holes overlap or touch");
        }
        if (hole.is_counterclockwise_oriented()) hole.reverse_orientation();
        holes.push_back(std::move(hole));
      }
      return Domain(outer, holes.begin(), holes.end());
    }

    // Align nearby underpass boundaries with the fixed footprint.
    Domain snap_to_footprint(const Domain& polygon,
                             const std::vector<Segment_2>& footprint,
                             float tolerance) {
      const EPECK::FT tolerance_sq = double(tolerance) * tolerance;
      std::set<Point_2> vertices;
      for (const auto& edge : footprint) {
        vertices.insert(edge.source());
        vertices.insert(edge.target());
      }
      // Prefer footprint vertices; otherwise project onto a nearby edge.
      auto snap = [&](Point_2 p) {
        auto best = tolerance_sq;
        std::optional<Point_2> nearest;
        for (const auto& v : vertices) {
          auto distance = CGAL::squared_distance(p, v);
          if (distance <= best) {
            best = distance;
            nearest = v;
          }
        }
        if (nearest) return *nearest;
        for (const auto& edge : footprint) {
          auto q = edge.supporting_line().projection(p);
          if (!edge.has_on(q)) continue;
          auto distance = CGAL::squared_distance(p, q);
          if (distance <= best) {
            best = distance;
            nearest = q;
          }
        }
        return nearest.value_or(p);
      };
      // Rebuild each ring after snapping and adding nearby footprint vertices.
      auto ring = [&](const Polygon& input) {
        std::vector<Point_2> snapped;
        for (const auto& p : input) {
          auto q = snap(p);
          if (snapped.empty() || snapped.back() != q) snapped.push_back(q);
        }
        if (snapped.size() > 1 && snapped.front() == snapped.back())
          snapped.pop_back();
        Polygon output;
        for (std::size_t i = 0; i < snapped.size(); ++i) {
          auto p = snapped[i], q = snapped[(i + 1) % snapped.size()];
          if (p == q) continue;
          Segment_2 segment(p, q);
          std::vector<std::pair<EPECK::FT, Point_2>> splits;
          const auto direction = q - p;
          const auto length_sq = direction.squared_length();
          splits.emplace_back(0, p);
          // Route close crossings through existing footprint vertices. This
          // avoids adding a sub-tolerance split beside an existing roof vertex.
          for (const auto& v : vertices) {
            auto t = CGAL::scalar_product(v - p, direction) / length_sq;
            if (t > 0 && t < 1 &&
                CGAL::squared_distance(v, segment) <= tolerance_sq)
              splits.emplace_back(t, v);
          }
          std::sort(splits.begin(), splits.end());
          for (const auto& [t, point] : splits)
            if (output.is_empty() || output[output.size() - 1] != point)
              output.push_back(point);
        }
        if (output.size() < 3 || !output.is_simple() || output.area() == 0)
          throw rooferException(
              "underpass collapses at the extrusion tolerance");
        return output;
      };
      auto outer = ring(polygon.outer_boundary());
      std::vector<Polygon> holes;
      for (const auto& hole : polygon.holes()) holes.push_back(ring(hole));
      return Domain(outer, holes.begin(), holes.end());
    }

    // Insert exterior and hole edges; the arrangement splits intersections.
    void insert_polygon(Arrangement_2& arr, const Domain& polygon) {
      auto insert_ring = [&](const Polygon& ring) {
        for (const auto& e : ring.edges()) CGAL::insert(arr, e);
      };
      insert_ring(polygon.outer_boundary());
      for (const auto& hole : polygon.holes()) insert_ring(hole);
    }

    // Visit vertices on every boundary component of a bounded face.
    template <typename Callback>
    void visit_points(Face_const_handle face, Callback&& visit) {
      auto ring = [&](auto first) {
        auto e = first;
        do {
          visit(e->source()->point());
        } while (++e != first);
      };
      ring(face->outer_ccb());
      for (auto hole :
           CGAL::make_range(face->inner_ccbs_begin(), face->inner_ccbs_end()))
        ring(hole);
    }

    // Identify footprint halfedges with the building interior on their left.
    bool boundary(Halfedge_const_handle e) {
      return e->face()->data().in_footprint &&
             !e->twin()->face()->data().in_footprint;
    }

    // Follow the footprint boundary past any incident interior roof edges.
    Halfedge_handle next_boundary(Halfedge_handle e) {
      auto next = e->next();
      while (next->twin()->face()->data().in_footprint)
        next = next->twin()->next();
      return next;
    }

    std::vector<std::vector<Halfedge_handle>> boundary_loops(
        Arrangement_2& arr) {
      std::set<Halfedge_handle> visited;
      std::vector<std::vector<Halfedge_handle>> loops;
      for (auto edge : arr.halfedge_handles()) {
        if (!boundary(edge) || visited.contains(edge)) continue;
        auto& loop = loops.emplace_back();
        auto e = edge;
        do {
          loop.push_back(e);
          visited.insert(e);
          e = next_boundary(e);
        } while (e != edge);
      }
      return loops;
    }

    // Ordinary floors use the roof's footprint loops directly, without another
    // arrangement. Attach each courtyard to its containing exterior in its
    // part.
    void emit_ground(Arrangement_2& roof, const ElevationProvider& elevations,
                     std::unordered_map<int, Mesh>& meshes) {
      struct Ground {
        int part;
        Polygon domain;
        LinearRing polygon;
      };
      std::vector<Ground> exteriors, holes;
      for (const auto& loop : boundary_loops(roof)) {
        Ground ground{loop.front()->face()->data().part_id, {}, {}};
        for (auto e : loop) {
          const auto& p = e->source()->point();
          ground.domain.push_back(p);
          append(ground.polygon, lifted(p, elevations.get(p)));
        }
        std::reverse(ground.polygon.begin(), ground.polygon.end());
        auto& output =
            ground.domain.is_counterclockwise_oriented() ? exteriors : holes;
        output.push_back(std::move(ground));
      }
      for (auto& hole : holes)
        for (auto& exterior : exteriors)
          if (hole.part == exterior.part &&
              exterior.domain.bounded_side(hole.domain[0]) ==
                  CGAL::ON_BOUNDED_SIDE) {
            exterior.polygon.interior_rings().push_back(
                std::move(hole.polygon));
            break;
          }
      for (auto& exterior : exteriors)
        meshes[exterior.part].push_polygon(exterior.polygon, 0);
    }
  }  // namespace

  namespace {
    struct Candidate {
      Domain polygon;
      std::size_t input_index;
      float height, bound = 0;
      bool enabled = true;
    };
    struct Cell {
      std::size_t roof;
      std::vector<std::size_t> covered;
    };
    using Cells = std::unordered_map<Face_const_handle, Cell>;
    struct SplitObserver : CGAL::Arr_observer<Arrangement_2> {
      Cells& cells;
      SplitObserver(Arrangement_2& arr, Cells& cells)
          : CGAL::Arr_observer<Arrangement_2>(arr), cells(cells) {}
      void after_split_face(Face_handle old, Face_handle fresh, bool) override {
        fresh->set_data(old->data());
        cells[fresh] = cells.at(old);
      }
    };
    using Loop = std::vector<Halfedge_handle>;
    // Trace the boundary of a logical surface without deleting graph edges
    // that may still be needed by a different surface.
    template <typename Group>
    std::map<int, std::vector<Loop>> surface_loops(Arrangement_2& arr,
                                                   Group group) {
      std::map<int, std::vector<Loop>> result;
      std::set<Halfedge_handle> visited;
      for (auto e : arr.halfedge_handles()) {
        const int id = group(e->face());
        if (id < 0 || group(e->twin()->face()) == id || visited.contains(e))
          continue;
        auto& loop = result[id].emplace_back();
        auto next = e;
        do {
          visited.insert(next);
          loop.push_back(next);
          next = next->next();
          while (group(next->twin()->face()) == id) next = next->twin()->next();
        } while (next != e);
      }
      return result;
    }
    struct Surface {
      Domain domain;
      Loop exterior;
      std::vector<Loop> holes;
    };
    std::vector<Surface> surfaces(const std::vector<Loop>& loops) {
      std::vector<Surface> result;
      std::vector<std::pair<Polygon, Loop>> holes;
      for (const auto& loop : loops) {
        Polygon polygon;
        for (auto e : loop) polygon.push_back(e->source()->point());
        if (polygon.is_counterclockwise_oriented())
          result.push_back({Domain(polygon), loop, {}});
        else
          holes.emplace_back(polygon, loop);
      }
      for (auto& [polygon, loop] : holes) {
        Surface* parent = nullptr;
        for (auto& s : result)
          if (s.domain.outer_boundary().bounded_side(polygon[0]) ==
                  CGAL::ON_BOUNDED_SIDE &&
              (!parent || s.domain.outer_boundary().area() <
                              parent->domain.outer_boundary().area()))
            parent = &s;
        if (parent) {
          parent->domain.add_hole(polygon);
          parent->holes.push_back(loop);
        }
      }
      return result;
    }
    struct Region {
      int part = -1, winner = -1;
      float ceiling = 0;
    };
    using Regions = std::unordered_map<Face_const_handle, Region>;

    EPECK::FT exact_roof_z(Face_const_handle f, const Point_2& p) {
      // Evaluate exact coefficients so contact points round to precisely the
      // same float on the roof and on the ceiling.
      const auto& pl = f->data().plane;
      return -(EPECK::FT(pl.a()) * p.x() + EPECK::FT(pl.b()) * p.y() + pl.d()) /
             EPECK::FT(pl.c());
    }
    float roof_z(Face_const_handle f, const Point_2& p) {
      return float(CGAL::to_double(exact_roof_z(f, p)));
    }
    std::vector<Segment_2> contact_segments(Face_const_handle f, float height) {
      const auto& plane = f->data().plane;
      if (plane.a() == 0 && plane.b() == 0) return {};
      EPECK::Line_2 line(EPECK::FT(plane.a()), EPECK::FT(plane.b()),
                         EPECK::FT(plane.c()) * EPECK::FT(height) + plane.d());
      const auto domain = arr_face_polygon(f);
      std::vector<Point_2> points;
      auto boundary = [&](const Polygon& ring) {
        for (const auto& edge : ring.edges()) {
          auto intersection = CGAL::intersection(line, edge);
          if (!intersection) continue;
          if (auto point = std::get_if<Point_2>(&*intersection))
            points.push_back(*point);
          else if (auto segment =
                       std::get_if<EPECK::Segment_2>(&*intersection)) {
            points.push_back(segment->source());
            points.push_back(segment->target());
          }
        }
      };
      boundary(domain.outer_boundary());
      for (const auto& hole : domain.holes()) boundary(hole);
      const auto direction = line.to_vector();
      std::sort(points.begin(), points.end(),
                [&](const auto& a, const auto& b) {
                  return CGAL::scalar_product(a - CGAL::ORIGIN, direction) <
                         CGAL::scalar_product(b - CGAL::ORIGIN, direction);
                });
      points.erase(std::unique(points.begin(), points.end()), points.end());
      std::vector<Segment_2> segments;
      for (std::size_t i = 1; i < points.size(); ++i)
        if (polygon_contains(domain, CGAL::midpoint(points[i - 1], points[i]),
                             PolygonBoundaryPolicy::strict_interior))
          segments.emplace_back(points[i - 1], points[i]);
      return segments;
    }
    float lower_z(const Region& r, const ElevationProvider& elevations,
                  const Point_2& p) {
      return r.winner < 0 ? elevations.get(p) : r.ceiling;
    }
    // Establish ground and ceiling contacts before snapping incident roofs.
    void split_lower_profiles(Arrangement_2& arr, const Regions& regions,
                              const ElevationProvider& elevations) {
      std::vector<Point_2> points;
      for (auto e : arr.edge_handles()) {
        auto p = e->source()->point(), q = e->target()->point();
        std::vector<HeightProfile> roofs, lower;
        for (auto f : {e->face(), e->twin()->face()}) {
          if (!f->data().in_footprint) continue;
          const auto& r = regions.at(f);
          roofs.push_back({exact_roof_z(f, p), exact_roof_z(f, q)});
          lower.push_back(
              {lower_z(r, elevations, p), lower_z(r, elevations, q)});
        }
        auto split = [&](const HeightProfile& a, const HeightProfile& b) {
          if (auto point = profile_crossing(p, q, a, b))
            points.emplace_back(point->x(), point->y());
        };
        for (const auto& floor : lower)
          for (const auto& roof : roofs) split(floor, roof);
        if (lower.size() == 2) split(lower[0], lower[1]);
      }
      for (const auto& p : points) CGAL::insert_point(arr, p);
    }

    void split_roof_profiles(Arrangement_2& arr, const Regions& regions,
                             VertexColumns& columns) {
      std::vector<std::pair<Halfedge_handle, EPECK::Point_3>> crossings;
      for (auto e : arr.edge_handles()) {
        auto f = e->face(), n = e->twin()->face();
        if (regions.at(f).part < 0 || regions.at(n).part < 0) continue;
        const auto& source = columns.at(e->source());
        const auto& target = columns.at(e->target());
        if (auto point =
                profile_crossing(e->source()->point(), e->target()->point(),
                                 {source.height(f), target.height(f)},
                                 {source.height(n), target.height(n)}))
          crossings.emplace_back(e, *point);
      }
      for (const auto& [e, point] : crossings) {
        auto v = CGAL::insert_point(arr, Point_2(point.x(), point.y()));
        const float z = float(CGAL::to_double(point.z()));
        columns[v].heights = {{z, e->face()}, {z, e->twin()->face()}};
      }
    }
  }  // namespace

  void ExtrusionArrangement::compute(
      Arrangement_2& arr, const ElevationProvider& elevations,
      std::span<const Underpass> underpasses,
      const ArrangementExtruderConfig& cfg,
      std::vector<UnderpassDiagnostic>& diagnostics,
      std::unordered_map<int, Mesh>& meshes, WallAssembler& walls) {
    std::vector<Segment_2> footprint;
    for (auto e : arr.halfedge_handles())
      if (boundary(e))
        footprint.emplace_back(e->source()->point(), e->target()->point());
    Cells cells;
    std::size_t roof_id = 0;
    for (auto f : arr.face_handles()) cells.emplace(f, Cell{roof_id++, {}});
    SplitObserver observer(arr, cells);
    std::vector<Candidate> candidates;
    for (std::size_t i = 0; i < underpasses.size(); ++i) {
      try {
        const float h = float(underpasses[i].ceiling_elevation);
        if (!std::isfinite(h))
          throw rooferException("non-finite ceiling elevation");
        auto polygon = snap_to_footprint(input_polygon(underpasses[i].polygon),
                                         footprint, cfg.snap_tolerance);
        candidates.push_back({polygon, i, h});
        insert_polygon(arr, polygon);
      } catch (const std::exception& ex) {
        diagnostics.push_back({i, UnderpassDiagnosticCode::invalid_input,
                               ex.what(), std::nullopt});
      }
    }
    Location location(arr);
    std::vector<float> minimum(candidates.size(),
                               std::numeric_limits<float>::infinity());
    std::vector<float> maximum(candidates.size(),
                               -std::numeric_limits<float>::infinity());
    for (auto f : arr.face_handles()) {
      if (!f->data().in_footprint || f->is_unbounded()) continue;
      auto sample = arr_face_interior_point(f, location);
      if (!sample)
        throw rooferException("Unable to locate extrusion cell interior");
      for (std::size_t i = 0; i < candidates.size(); ++i)
        if (polygon_contains(candidates[i].polygon, *sample,
                             PolygonBoundaryPolicy::include_outer_boundary)) {
          cells.at(f).covered.push_back(i);
          visit_points(f, [&](const Point_2& p) {
            minimum[i] = std::min(minimum[i], roof_z(f, p));
          });
        }
    }
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      // Query each connected input domain once, independent of roof partitions.
      auto loops = surface_loops(arr, [&](Face_const_handle f) {
        const auto& covered = cells.at(f).covered;
        return std::find(covered.begin(), covered.end(), i) != covered.end()
                   ? 0
                   : -1;
      });
      for (auto& [id, boundaries] : loops)
        for (const auto& s : surfaces(boundaries))
          maximum[i] =
              std::max(maximum[i], elevations.get_max_elevation(s.domain));
      auto& c = candidates[i];
      if (!std::isfinite(minimum[i])) {
        c.enabled = false;
        continue;
      }
      // The bound also prevents alignment from lowering a supplied elevation.
      c.bound = c.height;
      if (underpasses[c.input_index].uses_default_clearance) {
        c.bound = std::min(c.height, maximum[i] + cfg.underpass_min_height);
        c.height = std::min(
            c.height, std::max(c.bound, minimum[i] - cfg.underpass_roof_margin -
                                            cfg.snap_tolerance));
      }
      if (c.height <= maximum[i] + cfg.snap_tolerance) {
        c.enabled = false;
      }
    }
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < candidates.size(); ++i)
      if (candidates[i].enabled) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
      return candidates[a].height < candidates[b].height;
    });
    std::optional<float> reference;
    for (auto i : order) {
      auto& c = candidates[i];
      if (reference && c.height - *reference <= cfg.snap_tolerance &&
          *reference >= c.bound && *reference > maximum[i] + cfg.snap_tolerance)
        c.height = *reference;
      else
        reference = c.height;
    }
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      const auto& c = candidates[i];
      if (!std::isfinite(minimum[i])) continue;
      if (!c.enabled)
        diagnostics.push_back(
            {c.input_index, UnderpassDiagnosticCode::insufficient_clearance,
             "no floor clearance below the roof", std::nullopt});
      else if (c.height < float(underpasses[c.input_index].ceiling_elevation))
        diagnostics.push_back({c.input_index,
                               UnderpassDiagnosticCode::ceiling_lowered,
                               {},
                               c.height});
    }
    // Insert every eligible candidate's contact segments before retries. A
    // rejected winner can then expose another ceiling without rebuilding.
    std::vector<Segment_2> contacts;
    for (auto f : arr.face_handles()) {
      if (!f->data().in_footprint) continue;
      for (auto i : cells.at(f).covered) {
        if (!candidates[i].enabled) continue;
        auto segments = contact_segments(f, candidates[i].height);
        contacts.insert(contacts.end(), segments.begin(), segments.end());
      }
    }
    for (const auto& segment : contacts) CGAL::insert(arr, segment);

    Regions regions;
    auto classify = [&] {
      regions.clear();
      Location locate(arr);
      for (auto f : arr.face_handles()) {
        Region r;
        if (f->data().in_footprint) {
          r.part = f->data().part_id;
          for (auto i : cells.at(f).covered)
            if (candidates[i].enabled &&
                (r.winner < 0 || candidates[i].height > r.ceiling)) {
              r.winner = int(i);
              r.ceiling = candidates[i].height;
            }
          if (r.winner >= 0) {
            auto sample = arr_face_interior_point(f, locate);
            if (!sample)
              throw rooferException("Unable to locate carved cell interior");
            if (exact_roof_z(f, *sample) <= EPECK::FT(r.ceiling)) r.part = -1;
            float maximum_roof = -std::numeric_limits<float>::infinity();
            visit_points(f, [&](const Point_2& p) {
              maximum_roof = std::max(maximum_roof, roof_z(f, p));
            });
            // An exact positive sliver may disappear in mesh coordinates.
            if (maximum_roof <= r.ceiling) r.part = -1;
          }
        }
        regions.emplace(f, r);
      }
    };
    auto sources = [&](Halfedge_const_handle e, std::set<std::size_t>& out) {
      const auto& a = cells.at(e->face()).covered;
      const auto& b = cells.at(e->twin()->face()).covered;
      for (auto i : a)
        if (candidates[i].enabled &&
            std::find(b.begin(), b.end(), i) == b.end())
          out.insert(i);
      for (auto i : b)
        if (candidates[i].enabled &&
            std::find(a.begin(), a.end(), i) == a.end())
          out.insert(i);
      // A contact boundary can be generated inside a single input, rather
      // than by its polygon outline. Attribute such failures to its winner.
      const auto& ra = regions.at(e->face());
      const auto& rb = regions.at(e->twin()->face());
      if ((ra.part < 0) != (rb.part < 0))
        for (int winner : {ra.winner, rb.winner})
          if (winner >= 0 && candidates[winner].enabled) out.insert(winner);
    };
    auto lower_groups = [&](bool preserve_attributes) {
      std::map<std::tuple<int, std::optional<float>, int>, int> ids;
      std::unordered_map<Face_const_handle, int> groups;
      for (auto f : arr.face_handles()) {
        const auto& r = regions.at(f);
        if (r.part < 0) {
          groups[f] = -1;
          continue;
        }
        // Attribute boundaries partition exported surfaces only. They do not
        // change the physical underside or which inputs geometry checks reject.
        int attribute_owner = -1;
        if (preserve_attributes && r.winner >= 0 &&
            !underpasses[candidates[r.winner].input_index]
                 .attributes._attributes.empty())
          attribute_owner = r.winner;
        auto key = std::make_tuple(
            r.part, r.winner < 0 ? std::optional<float>{} : r.ceiling,
            attribute_owner);
        auto [it, inserted] = ids.emplace(key, int(ids.size()));
        groups[f] = it->second;
      }
      return groups;
    };
    for (;;) {
      classify();
      std::set<std::size_t> invalid;
      // Two surviving cells may touch only along a line where an underpass
      // ceiling meets a roof step or a valley. That is a pinched boundary,
      // rather than a positive-volume connection between their solids.
      for (auto edge : arr.edge_handles()) {
        const auto a = edge->face(), b = edge->twin()->face();
        const auto& ra = regions.at(a);
        const auto& rb = regions.at(b);
        if (ra.part < 0 || rb.part < 0 || ra.part != rb.part) continue;
        const auto p =
            CGAL::midpoint(edge->source()->point(), edge->target()->point());
        if (std::max(lower_z(ra, elevations, p), lower_z(rb, elevations, p)) ==
            std::min(roof_z(a, p), roof_z(b, p))) {
          for (int winner : {ra.winner, rb.winner})
            if (winner >= 0) invalid.insert(winner);
        }
      }
      // At every height, incident solid sectors must form one contiguous arc.
      for (auto v : arr.vertex_handles()) {
        if (v->is_isolated() || v->degree() < 4) continue;
        std::vector<std::pair<float, float>> intervals;
        std::vector<float> levels;
        auto first = v->incident_halfedges(), e = first;
        do {
          auto f = e->face();
          const auto& r = regions.at(f);
          float lo = r.part < 0 ? 0 : lower_z(r, elevations, v->point());
          float hi = r.part < 0 ? 0 : roof_z(f, v->point());
          intervals.emplace_back(lo, hi);
          levels.push_back(lo);
          levels.push_back(hi);
        } while (++e != first);
        std::sort(levels.begin(), levels.end());
        for (std::size_t i = 1; i < levels.size(); ++i) {
          if (levels[i] == levels[i - 1]) continue;
          const double z = (double(levels[i]) + levels[i - 1]) / 2;
          int transitions = 0;
          for (std::size_t j = 0; j < intervals.size(); ++j) {
            auto solid = [&](std::size_t k) {
              return intervals[k].first < z && z < intervals[k].second;
            };
            transitions += solid(j) != solid((j + 1) % intervals.size());
          }
          if (transitions > 2) {
            do {
              sources(e, invalid);
            } while (++e != first);
            break;
          }
        }
      }
      auto groups = lower_groups(false);
      auto loops =
          surface_loops(arr, [&](Face_const_handle f) { return groups.at(f); });
      std::set<std::size_t> bad_ceiling, bad_ground, short_ceiling,
          short_ground;
      std::set<std::size_t> isolated_ceiling, isolated_ground;
      for (auto& [id, boundaries] : loops) {
        const bool ceiling =
            regions.at(boundaries.front().front()->face()).winner >= 0;
        auto& bad = ceiling ? bad_ceiling : bad_ground;
        auto& isolated = ceiling ? isolated_ceiling : isolated_ground;
        auto& short_edges = ceiling ? short_ceiling : short_ground;
        struct Edge {
          Segment_2 segment;
          std::vector<Halfedge_handle> sources;
        };
        std::vector<std::vector<Edge>> rings;
        std::vector<int> components;
        int component = 0;
        for (const auto& surface : surfaces(boundaries)) {
          std::vector<Loop> surface_rings{surface.exterior};
          surface_rings.insert(surface_rings.end(), surface.holes.begin(),
                               surface.holes.end());
          for (const auto& loop : surface_rings) {
            components.push_back(component);
            auto& ring = rings.emplace_back();
            // Ignore collinear computational subdivisions when measuring width.
            std::size_t start = 0;
            while (start < loop.size() &&
                   CGAL::collinear(loop[(start + loop.size() - 1) % loop.size()]
                                       ->source()
                                       ->point(),
                                   loop[start]->source()->point(),
                                   loop[start]->target()->point()))
              ++start;
            for (std::size_t k = 0; k < loop.size(); ++k) {
              auto e = loop[(start + k) % loop.size()];
              if (!ring.empty() &&
                  CGAL::collinear(ring.back().segment.source(),
                                  e->source()->point(), e->target()->point())) {
                ring.back().segment = Segment_2(ring.back().segment.source(),
                                                e->target()->point());
                ring.back().sources.push_back(e);
              } else
                ring.push_back(
                    {Segment_2(e->source()->point(), e->target()->point()),
                     {e}});
            }
          }
          ++component;
        }
        auto rounded = [](const Point_2& p) {
          return Point_2(float(CGAL::to_double(p.x())),
                         float(CGAL::to_double(p.y())));
        };
        for (auto& ring : rings)
          for (auto& edge : ring)
            edge.segment = Segment_2(rounded(edge.segment.source()),
                                     rounded(edge.segment.target()));
        auto add = [&](const Edge& edge, std::set<std::size_t>& out) {
          for (auto e : edge.sources) sources(e, out);
        };
        const double tolerance_sq =
            double(cfg.snap_tolerance) * cfg.snap_tolerance;
        for (std::size_t a = 0; a < rings.size(); ++a)
          for (std::size_t i = 0; i < rings[a].size(); ++i) {
            const auto& x = rings[a][i];
            if (CGAL::to_double(x.segment.squared_length()) <= tolerance_sq)
              add(x, short_edges);
            for (std::size_t b = a; b < rings.size(); ++b)
              for (std::size_t j = b == a ? i + 1 : 0; j < rings[b].size();
                   ++j) {
                if (a == b &&
                    (j == i + 1 || (i == 0 && j + 1 == rings[a].size())))
                  continue;
                // Separate patches may share a genuine footprint vertex.
                // Junction validation above already checks the solid sectors
                // there. Preserve checks on distinct but nearly coincident
                // vertices: rounding must not turn a tiny gap into contact.
                if (components[a] != components[b]) {
                  const auto& y = rings[b][j];
                  auto p = x.sources.front()->source()->point();
                  auto q = x.sources.back()->target()->point();
                  auto r = y.sources.front()->source()->point();
                  auto t = y.sources.back()->target()->point();
                  if (p == r || p == t || q == r || q == t) continue;
                }
                if (CGAL::to_double(CGAL::squared_distance(
                        x.segment, rings[b][j].segment)) <= tolerance_sq) {
                  std::set<std::size_t> implicated;
                  add(x, implicated);
                  add(rings[b][j], implicated);
                  bad.insert(implicated.begin(), implicated.end());
                  if (implicated.size() == 1)
                    isolated.insert(*implicated.begin());
                }
              }
          }
      }
      for (const auto* set : {&short_ceiling, &isolated_ceiling, &bad_ceiling,
                              &short_ground, &isolated_ground, &bad_ground})
        if (!set->empty()) {
          invalid.insert(set->begin(), set->end());
          break;
        }
      if (invalid.empty()) break;
      for (auto i : invalid) {
        candidates[i].enabled = false;
        diagnostics.push_back(
            {candidates[i].input_index,
             UnderpassDiagnosticCode::invalid_underside_geometry,
             "non-manifold or sub-tolerance underside geometry", std::nullopt});
      }
    }
    split_lower_profiles(arr, regions, elevations);
    // Edge splitting does not introduce faces, so region handles remain valid.

    // Match ordinary extrusion's roof-height equalization where there is
    // ample clearance. Exact ceiling contacts stay fixed; otherwise nearly
    // equal incident roof planes can produce zero-length exported step edges.
    VertexColumns columns;
    for (auto v : arr.vertex_handles()) {
      if (v->is_isolated()) continue;
      auto& column = columns[v];
      float maximum_lower = -std::numeric_limits<float>::infinity();
      float minimum_upper = std::numeric_limits<float>::infinity();
      auto first = v->incident_halfedges(), e = first;
      do {
        auto f = e->face();
        const auto& r = regions.at(f);
        if (r.part >= 0) {
          float h = roof_z(f, v->point());
          column.heights.emplace_back(h, f);
          minimum_upper = std::min(minimum_upper, h);
          maximum_lower =
              std::max(maximum_lower, lower_z(r, elevations, v->point()));
        }
      } while (++e != first);
      if (minimum_upper > maximum_lower + cfg.snap_tolerance)
        column.snap(cfg.snap_tolerance);
    }
    split_roof_profiles(arr, regions, columns);

    // Assign connected positive-volume components deterministic part IDs.
    int next_part = 0;
    for (auto f : arr.face_handles())
      next_part = std::max(next_part, f->data().part_id + 1);
    std::set<Face_const_handle> visited;
    std::set<int> used_parts;
    std::vector<Face_handle> seeds;
    for (auto f : arr.face_handles())
      if (regions.at(f).part >= 0) seeds.push_back(f);
    std::sort(seeds.begin(), seeds.end(), [](auto a, auto b) {
      auto minimum = [](auto f) {
        Point_2 p = f->outer_ccb()->source()->point();
        visit_points(f, [&](auto q) {
          if (q < p) p = q;
        });
        return p;
      };
      return minimum(a) < minimum(b);
    });
    for (auto seed : seeds) {
      if (visited.contains(seed)) continue;
      const int original = regions.at(seed).part;
      const int part =
          used_parts.insert(original).second ? original : next_part++;
      std::vector<Face_handle> component{seed};
      std::set<Face_const_handle> marked{seed};
      for (std::size_t k = 0; k < component.size(); ++k) {
        auto f = component[k];
        auto scan = [&](auto first) {
          auto e = first;
          do {
            auto n = e->twin()->face();
            if (!marked.contains(n) && regions.at(n).part == original) {
              auto p =
                  CGAL::midpoint(e->source()->point(), e->target()->point());
              if (std::max(lower_z(regions.at(f), elevations, p),
                           lower_z(regions.at(n), elevations, p)) <
                  std::min(roof_z(f, p), roof_z(n, p))) {
                marked.insert(n);
                component.push_back(n);
              }
            }
          } while (++e != first);
        };
        scan(f->outer_ccb());
        for (auto h :
             CGAL::make_range(f->inner_ccbs_begin(), f->inner_ccbs_end()))
          scan(h);
      }
      for (auto f : component) {
        regions.at(f).part = part;
        visited.insert(f);
      }
    }

    auto emit_surface = [&](const Surface& s, int part, int label, auto height,
                            const AttributeMapRow* attributes) {
      auto ring = [&](const Loop& loop) {
        vec3f points;
        for (auto e : loop)
          append(points, lifted(e->source()->point(), height(e)));
        if (label != 1) std::reverse(points.begin(), points.end());
        return points;
      };
      LinearRing polygon;
      auto exterior = ring(s.exterior);
      polygon.insert(polygon.end(), exterior.begin(), exterior.end());
      for (const auto& hole : s.holes)
        polygon.interior_rings().push_back(ring(hole));
      auto& mesh = meshes[part];
      mesh.push_polygon(polygon, label);
      if (attributes) {
        mesh.get_attributes().resize(mesh.get_polygons().size());
        mesh.get_attributes().back() = *attributes;
      }
    };
    if (cfg.generate_roofs) {
      std::map<std::pair<int, std::size_t>, int> ids;
      auto loops = surface_loops(arr, [&](Face_const_handle f) {
        const auto& r = regions.at(f);
        if (r.part < 0) return -1;
        return ids
            .emplace(std::make_pair(r.part, cells.at(f).roof), int(ids.size()))
            .first->second;
      });
      for (auto& [id, boundaries] : loops)
        for (const auto& s : surfaces(boundaries))
          emit_surface(
              s, regions.at(s.exterior.front()->face()).part, 1,
              [&](auto e) { return columns.at(e->source()).height(e->face()); },
              nullptr);
    }
    if (cfg.generate_floor) {
      auto groups = lower_groups(true);
      auto loops =
          surface_loops(arr, [&](Face_const_handle f) { return groups.at(f); });
      for (auto& [id, boundaries] : loops)
        for (const auto& s : surfaces(boundaries)) {
          const auto& r = regions.at(s.exterior.front()->face());
          const AttributeMapRow* attributes =
              r.winner < 0
                  ? nullptr
                  : &underpasses[candidates[r.winner].input_index].attributes;
          emit_surface(
              s, r.part, r.winner < 0 ? 0 : 4,
              [&](auto e) {
                return lower_z(r, elevations, e->source()->point());
              },
              attributes);
        }
    }
    if (!cfg.generate_walls) return;
    for (auto e : arr.halfedge_handles()) {
      auto f = e->face(), n = e->twin()->face();
      const auto& r = regions.at(f);
      const auto& other = regions.at(n);
      if (r.part < 0) continue;
      const auto p = e->source()->point(), q = e->target()->point();
      auto at = [&](Vertex_const_handle v) {
        const auto& column = columns.at(v);
        const float lo = lower_z(r, elevations, v->point()),
                    hi = column.height(f);
        const float nl =
            other.part < 0 ? hi : lower_z(other, elevations, v->point());
        const float nh = other.part < 0 ? hi : column.height(n);
        return std::array<float, 4>{lo, std::min(hi, nl), std::max(lo, nh), hi};
      };
      auto a = at(e->source()), b = at(e->target());
      const int label = n->data().in_footprint ? 3 : 2;
      if (a[1] > a[0] || b[1] > b[0])
        walls.add(p, q, {a[0], b[0]}, {a[1], b[1]}, r.part, label);
      if (other.part >= 0 && (a[3] > a[2] || b[3] > b[2]))
        walls.add(p, q, {a[2], b[2]}, {a[3], b[3]}, r.part, label);
    }
  }

  void ExtrusionArrangement::emit_ground_and_facades(
      Arrangement_2& roof, const ElevationProvider& elevations,
      const ArrangementExtruderConfig& cfg,
      const std::function<float(Vertex_handle, Face_handle)>& roof_height,
      std::unordered_map<int, Mesh>& meshes, WallAssembler& walls) const {
    if (cfg.generate_floor) emit_ground(roof, elevations, meshes);
    if (cfg.generate_walls)
      for (auto e : roof.halfedge_handles())
        if (boundary(e)) {
          auto p = e->source()->point(), q = e->target()->point();
          walls.add(p, q, {elevations.get(p), elevations.get(q)},
                    {roof_height(e->source(), e->face()),
                     roof_height(e->target(), e->face())},
                    e->face()->data().part_id, 2);
        }
  }
}  // namespace roofer::reconstruction::detail
