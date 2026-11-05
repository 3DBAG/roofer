#include "FloorArrangement.hpp"
#include "ExtrusionUtils.hpp"

#include <CGAL/Arr_naive_point_location.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>
#include <map>
#include <set>

namespace roofer::reconstruction::detail {
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

    // Prefer an interior footprint face when a sample hits an edge or vertex.
    Face_const_handle located_face(const Location& locate, const Point_2& p) {
      auto object = locate.locate(p);
      if (const auto* face = std::get_if<Face_const_handle>(&object))
        return *face;
      if (const auto* edge = std::get_if<Halfedge_const_handle>(&object)) {
        return (*edge)->face()->data().in_footprint ? (*edge)->face()
                                                    : (*edge)->twin()->face();
      }
      if (const auto* vertex = std::get_if<Vertex_const_handle>(&object)) {
        auto first = (*vertex)->incident_halfedges(), edge = first;
        do {
          if (edge->face()->data().in_footprint) return edge->face();
        } while (++edge != first);
        return edge->face();
      }
      throw rooferException("Floor sample lies on a roof constraint");
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

    // Use the raised ceiling where present, otherwise the original floor.
    float lower_z(const FloorRegion& region,
                  const ElevationProvider& elevations, const Point_2& p) {
      return region.ceiling ? *region.ceiling : elevations.get(p);
    }

    struct Candidate {
      Domain polygon;
      std::size_t input_index;
      float height;
      bool enabled = true;
    };

    using EdgeKey = std::pair<Point_2, Point_2>;
    using BoundarySources = std::map<EdgeKey, std::vector<std::size_t>>;

    EdgeKey edge_key(Halfedge_const_handle e) {
      auto p = e->source()->point(), q = e->target()->point();
      return p < q ? EdgeKey{p, q} : EdgeKey{q, p};
    }

    // Dissolving rejected constraints leaves collinear split vertices behind.
    // Retain original roof boundary vertices so the two profiles still match.
    void merge_collinear(Arrangement_2& floor, BoundarySources& sources,
                         const std::set<Point_2>& footprint_vertices) {
      for (auto it = floor.vertices_begin(); it != floor.vertices_end();) {
        Vertex_handle vertex = it++;
        if (vertex->degree() != 2 ||
            footprint_vertices.contains(vertex->point()))
          continue;
        auto incident = vertex->incident_halfedges();
        Halfedge_handle a = incident, b = (++incident)->twin();
        const auto p = a->source()->point(), q = b->target()->point();
        if (!CGAL::collinear(p, vertex->point(), q)) continue;
        auto values = sources.at(edge_key(a));
        const auto& other = sources.at(edge_key(b));
        values.insert(values.end(), other.begin(), other.end());
        auto merged = floor.merge_edge(a, b, Segment_2(p, q));
        sources[edge_key(merged)] = std::move(values);
      }
    }

    // Find cutouts whose floor regions would collapse at the mesh tolerance.
    template <typename AddSources>
    std::set<std::size_t> sub_tolerance_regions(
        Arrangement_2& floor,
        const std::unordered_map<Face_const_handle, FloorRegion>& regions,
        float tolerance, AddSources&& add_sources) {
      const double tolerance_sq = double(tolerance) * tolerance;
      std::set<std::size_t> invalid_ceiling, invalid_ground;
      std::set<std::size_t> isolated_ceiling, isolated_ground;
      std::set<std::size_t> short_ceiling, short_ground;
      for (auto face : floor.face_handles()) {
        const auto& owner = regions.at(face);
        if (owner.part < 0) continue;
        struct BoundaryEdge {
          Halfedge_const_handle handle;
          Segment_2 segment;
        };
        auto rounded = [](const Point_2& p) {
          return Point_2(float(CGAL::to_double(p.x())),
                         float(CGAL::to_double(p.y())));
        };
        std::vector<std::vector<BoundaryEdge>> rings;
        auto ring = [&](auto first) {
          auto& edges = rings.emplace_back();
          auto e = first;
          do {
            edges.push_back({e, Segment_2(rounded(e->source()->point()),
                                          rounded(e->target()->point()))});
          } while (++e != first);
        };
        ring(face->outer_ccb());
        for (auto hole :
             CGAL::make_range(face->inner_ccbs_begin(), face->inner_ccbs_end()))
          ring(hole);
        auto& invalid = owner.ceiling ? invalid_ceiling : invalid_ground;
        auto& isolated = owner.ceiling ? isolated_ceiling : isolated_ground;
        auto& short_edges = owner.ceiling ? short_ceiling : short_ground;
        auto record_failure = [&](Halfedge_const_handle a,
                                  Halfedge_const_handle b) {
          std::set<std::size_t> sources;
          add_sources(a, sources);
          add_sources(b, sources);
          invalid.insert(sources.begin(), sources.end());
          if (sources.size() == 1) isolated.insert(*sources.begin());
        };
        for (std::size_t a = 0; a < rings.size(); ++a) {
          for (std::size_t i = 0; i < rings[a].size(); ++i) {
            const auto& first = rings[a][i].segment;
            if (first.squared_length() <= tolerance_sq)
              add_sources(rings[a][i].handle, short_edges);
            for (std::size_t b = a; b < rings.size(); ++b)
              for (std::size_t j = b == a ? i + 1 : 0; j < rings[b].size();
                   ++j) {
                if (a == b &&
                    (j == i + 1 || (i == 0 && j + 1 == rings[a].size())))
                  continue;
                if (CGAL::squared_distance(first, rings[b][j].segment) <=
                    tolerance_sq) {
                  record_failure(rings[a][i].handle, rings[b][j].handle);
                }
              }
          }
        }
      }
      // Short edges identify narrow inputs before nearby boundary fragments
      // implicate their neighbours. Then prefer contacts attributable to one
      // input over conflicts between inputs. Raised patches precede ground
      // gaps.
      if (!short_ceiling.empty()) return short_ceiling;
      if (!isolated_ceiling.empty()) return isolated_ceiling;
      if (!invalid_ceiling.empty()) return invalid_ceiling;
      if (!short_ground.empty()) return short_ground;
      return isolated_ground.empty() ? invalid_ground : isolated_ground;
    }

    // Find cutouts that would create non-manifold vertical edges at vertices.
    template <typename AddSources>
    std::set<std::size_t> invalid_junctions(
        Arrangement_2& floor,
        const std::unordered_map<Face_const_handle, FloorRegion>& regions,
        const ElevationProvider& elevations, AddSources&& add_sources) {
      std::set<std::size_t> invalid;
      for (auto vertex : floor.vertex_handles()) {
        if (vertex->degree() < 4) continue;
        std::vector<float> heights;
        auto first = vertex->incident_halfedges(), e = first;
        do {
          const auto& region = regions.at(e->face());
          heights.push_back(region.part < 0
                                ? std::numeric_limits<float>::infinity()
                                : lower_z(region, elevations, vertex->point()));
        } while (++e != first);
        auto levels = heights;
        std::sort(levels.begin(), levels.end());
        levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
        // Solid sectors must form one contiguous arc at each height interval.
        for (std::size_t i = 1; i < levels.size(); ++i) {
          const auto threshold = levels[i - 1];
          int transitions = 0;
          for (std::size_t j = 0; j < heights.size(); ++j)
            transitions += ((heights[j] <= threshold) !=
                            (heights[(j + 1) % heights.size()] <= threshold));
          if (transitions > 2) {
            do {
              add_sources(e, invalid);
            } while (++e != first);
            break;
          }
        }
      }
      return invalid;
    }

    struct FloorCell {
      int part = -1;
      std::vector<std::size_t> covered;
    };

    struct ClearanceOverlay {
      FaceInfo operator()(const FaceInfo& roof, const FaceInfo& floor) const {
        auto result = roof;
        result.label = floor.label;
        return result;
      }
    };

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

  // Build the underside, resolve ceiling heights, and align it with the roof.
  void FloorArrangement::prepare(
      Arrangement_2& roof, const ElevationProvider& elevations,
      std::span<const Underpass> underpasses,
      const ArrangementExtruderConfig& cfg,
      std::vector<UnderpassDiagnostic>& diagnostics) {
    floor.clear();
    regions.clear();
    if (!cfg.lod2 || underpasses.empty()) return;
    std::vector<Segment_2> footprint;
    std::set<Point_2> footprint_vertices;
    for (auto e : roof.halfedge_handles())
      if (boundary(e)) {
        footprint.emplace_back(e->source()->point(), e->target()->point());
        footprint_vertices.insert(e->source()->point());
      }

    std::vector<Candidate> candidates;
    for (std::size_t i = 0; i < underpasses.size(); ++i) {
      try {
        if (!std::isfinite(underpasses[i].ceiling_elevation))
          throw rooferException("non-finite ceiling elevation");
        candidates.push_back(
            {snap_to_footprint(input_polygon(underpasses[i].polygon), footprint,
                               cfg.snap_tolerance),
             i, float(underpasses[i].ceiling_elevation)});
      } catch (const std::exception& ex) {
        diagnostics.push_back({i, UnderpassDiagnosticCode::invalid_input,
                               ex.what(), std::nullopt});
      }
    }
    if (candidates.empty()) return;

    // Retain this subdivision and its coverage labels through rejection
    // retries.
    Arrangement_2 subdivision;
    for (const auto& segment : footprint) CGAL::insert(subdivision, segment);
    for (const auto& c : candidates) insert_polygon(subdivision, c.polygon);
    Location floor_location(subdivision), roof_location(roof);
    std::vector<FloorCell> cells;
    std::vector<float> minimum(candidates.size(),
                               std::numeric_limits<float>::infinity());
    std::vector<float> maximum(candidates.size(),
                               -std::numeric_limits<float>::infinity());
    for (auto face : subdivision.face_handles()) {
      face->data().label = cells.size();
      auto& cell = cells.emplace_back();
      if (face->is_unbounded()) continue;
      auto sample = arr_face_interior_point(face, floor_location);
      if (!sample)
        throw rooferException(
            "Unable to locate floor arrangement face interior");
      auto parent = located_face(roof_location, *sample);
      if (!parent->data().in_footprint) continue;
      cell.part = parent->data().part_id;
      for (std::size_t i = 0; i < candidates.size(); ++i)
        if (polygon_contains(candidates[i].polygon, *sample,
                             PolygonBoundaryPolicy::include_outer_boundary))
          cell.covered.push_back(i);
      if (cell.covered.empty()) continue;
      // Terrain queries depend only on floor cells, not on roof subdivisions.
      const auto max_z = elevations.get_max_elevation(arr_face_polygon(face));
      for (auto i : cell.covered) maximum[i] = std::max(maximum[i], max_z);
    }

    BoundarySources boundary_sources;
    for (auto e : subdivision.edge_handles()) {
      const auto& a = cells[e->face()->data().label].covered;
      const auto& b = cells[e->twin()->face()->data().label].covered;
      auto& sources = boundary_sources[edge_key(e)];
      std::set_symmetric_difference(a.begin(), a.end(), b.begin(), b.end(),
                                    std::back_inserter(sources));
    }

    // The overlay transfers roof planes and floor coverage without new samples
    // or repeated containment tests. Interior constraints never enter the roof.
    Arrangement_2 overlay;
    CGAL::Arr_face_overlay_traits<Arrangement_2, Arrangement_2, Arrangement_2,
                                  ClearanceOverlay>
        traits;
    CGAL::overlay(roof, subdivision, overlay, traits);
    for (auto face : overlay.face_handles()) {
      if (face->is_unbounded() || !face->data().in_footprint) continue;
      const auto& covered = cells[face->data().label].covered;
      if (covered.empty()) continue;
      float min_z = std::numeric_limits<float>::infinity();
      visit_points(face, [&](const Point_2& p) {
        min_z = std::min(min_z, float(plane_height(face->data().plane, p)));
      });
      for (auto i : covered) minimum[i] = std::min(minimum[i], min_z);
    }
    overlay.clear();
    // Keep one flat ceiling per input and discard cutouts without clearance.
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      auto& c = candidates[i];
      if (!std::isfinite(minimum[i])) {
        c.enabled = false;
        continue;
      }
      // Reserve the roof margin plus room for subsequent height snapping.
      const float limit =
          minimum[i] - cfg.underpass_roof_margin - cfg.snap_tolerance;
      if (c.height > limit) {
        diagnostics.push_back({c.input_index,
                               UnderpassDiagnosticCode::ceiling_lowered,
                               {},
                               limit});
        c.height = limit;
      }
      if (!std::isfinite(c.height) ||
          c.height <= maximum[i] + cfg.snap_tolerance) {
        c.enabled = false;
        diagnostics.push_back(
            {c.input_index, UnderpassDiagnosticCode::insufficient_clearance,
             "no floor clearance below the roof", std::nullopt});
      }
    }
    // Equalise nearby ceiling heights downward to avoid tiny vertical walls
    // while preserving every individual roof-clearance bound.
    std::map<float, float> canonical;
    for (const auto& c : candidates)
      if (c.enabled) canonical.emplace(c.height, c.height);
    if (!canonical.empty()) {
      float reference = canonical.begin()->first;
      for (auto& [height, value] : canonical) {
        if (height - reference > cfg.snap_tolerance) reference = height;
        value = reference;
      }
      for (std::size_t i = 0; i < candidates.size(); ++i) {
        auto& c = candidates[i];
        if (!c.enabled) continue;
        c.height = canonical.at(c.height);
        if (c.height <= maximum[i] + cfg.snap_tolerance) {
          c.enabled = false;
          diagnostics.push_back(
              {c.input_index, UnderpassDiagnosticCode::insufficient_clearance,
               "no floor clearance after height snapping", std::nullopt});
        }
      }
    }

    for (;;) {
      if (std::none_of(candidates.begin(), candidates.end(),
                       [](const auto& c) { return c.enabled; })) {
        floor.clear();
        regions.clear();
        return;
      }
      // Copy exact topology, then relabel. No intersections or point-location
      // queries need to be repeated when a candidate is disabled.
      floor = subdivision;
      regions.clear();
      for (auto face : floor.face_handles()) {
        const auto& cell = cells[face->data().label];
        FloorRegion region;
        region.part = cell.part;
        for (auto i : cell.covered)
          if (candidates[i].enabled)
            region.ceiling =
                std::max(region.ceiling.value_or(candidates[i].height),
                         candidates[i].height);
        face->data().in_footprint = region.part >= 0;
        face->data().part_id = region.part;
        regions.emplace(face, region);
      }
      // Keep coverage on the original cells. Merged face provenance is too
      // coarse to identify a small offending input on an otherwise valid patch.
      for (auto it = floor.edges_begin(); it != floor.edges_end();) {
        Halfedge_handle edge = it;
        ++it;
        auto a = edge->face(), b = edge->twin()->face();
        const auto ra = regions.at(a), rb = regions.at(b);
        if (ra.part == rb.part && ra.ceiling == rb.ceiling) {
          auto merged = floor.remove_edge(edge);
          regions.erase(a);
          regions.erase(b);
          regions[merged] = ra;
        }
      }
      auto sources = boundary_sources;
      merge_collinear(floor, sources, footprint_vertices);
      auto add_sources = [&](Halfedge_const_handle e,
                             std::set<std::size_t>& invalid) {
        const auto& a = regions.at(e->face());
        const auto& b = regions.at(e->twin()->face());
        for (auto i : sources.at(edge_key(e)))
          if (candidates[i].enabled && (a.ceiling == candidates[i].height ||
                                        b.ceiling == candidates[i].height))
            invalid.insert(i);
      };
      auto invalid = invalid_junctions(floor, regions, elevations, add_sources);
      auto small = sub_tolerance_regions(floor, regions, cfg.snap_tolerance,
                                         add_sources);
      invalid.insert(small.begin(), small.end());
      if (invalid.empty()) break;
      for (auto i : invalid) {
        candidates[i].enabled = false;
        diagnostics.push_back(
            {candidates[i].input_index,
             UnderpassDiagnosticCode::invalid_underside_geometry,
             "non-manifold or sub-tolerance underside geometry", std::nullopt});
      }
    }

    // Synchronise only the footprint boundary vertices.
    std::set<Point_2> points;
    for (auto e : floor.halfedge_handles())
      if (boundary(e)) points.insert(e->source()->point());
    for (const auto& point : points) CGAL::insert_point(roof, point);
  }

  // Emit ground, ceilings, and wall spans for shared polygon assembly.
  void FloorArrangement::emit(
      Arrangement_2& roof, const ElevationProvider& elevations,
      const ArrangementExtruderConfig& cfg,
      const std::function<float(Vertex_handle, Face_handle)>& roof_height,
      std::unordered_map<int, Mesh>& meshes, WallAssembler& walls) const {
    // Reverse the arrangement winding so ground and ceiling normals point down.
    if (cfg.generate_floor && floor.is_empty())
      emit_ground(roof, elevations, meshes);
    if (cfg.generate_floor && !floor.is_empty()) {
      for (auto face : floor.face_handles()) {
        const auto& region = regions.at(face);
        if (region.part < 0) continue;
        auto ring = [&](auto first) {
          vec3f points;
          auto e = first;
          do {
            auto p = e->source()->point();
            append(points, lifted(p, lower_z(region, elevations, p)));
          } while (++e != first);
          std::reverse(points.begin(), points.end());
          return points;
        };
        LinearRing polygon;
        auto outer = ring(face->outer_ccb());
        polygon.insert(polygon.end(), outer.begin(), outer.end());
        for (auto hole :
             CGAL::make_range(face->inner_ccbs_begin(), face->inner_ccbs_end()))
          polygon.interior_rings().push_back(ring(hole));
        meshes[region.part].push_polygon(polygon, region.ceiling ? 4 : 0);
      }
    }
    if (!cfg.generate_walls) return;

    // Walls around raised regions, including the hole rings forming pillars.
    for (auto edge : floor.edge_handles()) {
      Halfedge_const_handle e = edge;
      auto a = regions.at(e->face()), b = regions.at(e->twin()->face());
      if (a.part < 0 || b.part < 0 || a.ceiling == b.ceiling) continue;
      auto p = e->source()->point(), q = e->target()->point();
      // Traverse the lower face first so the wall points into the cutout.
      if (lower_z(a, elevations, CGAL::midpoint(p, q)) >
          lower_z(b, elevations, CGAL::midpoint(p, q))) {
        std::swap(a, b);
        e = e->twin();
        std::swap(p, q);
      }
      walls.add(p, q, {lower_z(a, elevations, p), lower_z(a, elevations, q)},
                {lower_z(b, elevations, p), lower_z(b, elevations, q)}, a.part,
                3);
    }

    // Both arrangements retain the same footprint splits. Match by oriented
    // exact endpoints rather than by DCEL handles belonging to another graph.
    std::map<EdgeKey, Face_const_handle> floor_edges;
    for (auto e : floor.halfedge_handles())
      if (boundary(e))
        floor_edges[{e->source()->point(), e->target()->point()}] = e->face();
    // Supply footprint spans to the same assembler as both kinds of steps.
    const FloorRegion ground;
    for (auto e : roof.halfedge_handles()) {
      if (!boundary(e)) continue;
      const auto p = e->source()->point(), q = e->target()->point();
      const FloorRegion* region = &ground;
      if (!floor.is_empty()) {
        auto f = floor_edges.find({p, q});
        if (f == floor_edges.end())
          throw rooferException("Roof/floor footprint boundary mismatch");
        region = &regions.at(f->second);
      }
      walls.add(
          p, q,
          {lower_z(*region, elevations, p), lower_z(*region, elevations, q)},
          {roof_height(e->source(), e->face()),
           roof_height(e->target(), e->face())},
          e->face()->data().part_id, 2);
    }
  }
}  // namespace roofer::reconstruction::detail
