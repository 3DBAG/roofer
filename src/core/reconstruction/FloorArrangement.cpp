#include "FloorArrangement.hpp"

#include <CGAL/Arr_naive_point_location.h>
#include <roofer/logger/logger.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace roofer::reconstruction::detail {
  namespace {
    using Polygon = CGAL::Polygon_2<EPECK>;
    using Domain = CGAL::Polygon_with_holes_2<EPECK>;
    using Location = CGAL::Arr_naive_point_location<Arrangement_2>;

    class EdgeObserver : public CGAL::Arr_observer<Arrangement_2> {
     public:
      using CGAL::Arr_observer<Arrangement_2>::Arr_observer;
      // Preserve facade IDs on both halves of a split roof boundary edge.
      void after_split_edge(Halfedge_handle a, Halfedge_handle b) override {
        b->data() = a->data();
        a->twin()->data() = b->twin()->data() = a->data();
      }
    };

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

    // Include the exterior boundary, but exclude holes and their boundaries.
    bool contains(const Domain& polygon, const Point_2& p) {
      if (polygon.outer_boundary().bounded_side(p) == CGAL::ON_UNBOUNDED_SIDE)
        return false;
      for (const auto& hole : polygon.holes())
        if (hole.bounded_side(p) != CGAL::ON_UNBOUNDED_SIDE) return false;
      return true;
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

    // Extract a face and its holes as a polygon for elevation queries.
    Domain face_domain(Face_const_handle face) {
      auto ring = [](auto first) {
        Polygon p;
        auto e = first;
        do {
          p.push_back(e->source()->point());
        } while (++e != first);
        return p;
      };
      auto outer = ring(face->outer_ccb());
      std::vector<Polygon> holes;
      for (auto hole :
           CGAL::make_range(face->inner_ccbs_begin(), face->inner_ccbs_end()))
        holes.push_back(ring(hole));
      return Domain(outer, holes.begin(), holes.end());
    }

    // Find an exact sample strictly inside a bounded arrangement face.
    Point_2 interior_point(Face_handle face, const Location& locate) {
      // An exact inward offset avoids float triangulation of narrow cells.
      auto e = face->outer_ccb();
      auto a = e->source()->point(), b = e->target()->point();
      auto middle = CGAL::midpoint(a, b);
      auto dx = b.x() - a.x(), dy = b.y() - a.y();
      EPECK::FT scale = 1;
      for (int i = 0; i < 256; ++i, scale /= 2) {
        Point_2 p(middle.x() - dy * scale, middle.y() + dx * scale);
        auto object = locate.locate(p);
        if (const auto* f = std::get_if<Face_const_handle>(&object);
            f && *f == face)
          return p;
      }
      throw rooferException("Unable to locate floor arrangement face interior");
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

    // Evaluate the roof plane, or the configured flat LoD 1 elevation.
    float roof_z(Face_const_handle face, const Point_2& p,
                 const ArrangementExtruderConfig& cfg) {
      if (!cfg.lod2)
        return cfg.lod1_extrude_to_max ? face->data().elevation_97p
                                       : face->data().elevation_70p;
      const auto& plane = face->data().plane;
      return float(-(plane.a() * CGAL::to_double(p.x()) +
                     plane.b() * CGAL::to_double(p.y()) + plane.d()) /
                   plane.c());
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

    // Check whether consecutive edges continue one facade within the same part.
    bool straight(Halfedge_handle a, Halfedge_handle b) {
      return a->face()->data().part_id == b->face()->data().part_id &&
             CGAL::collinear(a->source()->point(), a->target()->point(),
                             b->target()->point()) &&
             CGAL::scalar_product(a->target()->point() - a->source()->point(),
                                  b->target()->point() - b->source()->point()) >
                 0;
    }

    // Group exterior and courtyard boundaries into maximal straight facades.
    std::vector<std::vector<Halfedge_handle>> boundary_chains(
        Arrangement_2& arr, bool assign_ids = true) {
      std::set<Halfedge_handle> visited;
      std::vector<std::vector<Halfedge_handle>> chains;
      for (auto edge : arr.halfedge_handles()) {
        if (!boundary(edge) || visited.contains(edge)) continue;
        // Follow each complete boundary loop once.
        std::vector<Halfedge_handle> loop;
        auto e = edge;
        do {
          loop.push_back(e);
          visited.insert(e);
          e = next_boundary(e);
        } while (e != edge);
        // Start at a corner so a facade is not split across the loop's start.
        std::size_t start = 0;
        while (start < loop.size() &&
               straight(loop[(start + loop.size() - 1) % loop.size()],
                        loop[start]))
          ++start;
        if (start == loop.size())
          throw rooferException("Degenerate footprint loop");
        for (std::size_t i = 0; i < loop.size(); ++i) {
          e = loop[(start + i) % loop.size()];
          if (i == 0 || !straight(chains.back().back(), e))
            chains.emplace_back();
          chains.back().push_back(e);
          if (assign_ids)
            e->data().boundary_chain_id = e->twin()->data().boundary_chain_id =
                int(chains.size() - 1);
        }
      }
      return chains;
    }

    // Lift exact XY coordinates into the mesh's float coordinate format.
    arr3f lifted(const Point_2& p, float z) {
      return {float(CGAL::to_double(p.x())), float(CGAL::to_double(p.y())), z};
    }

    // Use the raised ceiling where present, otherwise the original floor.
    float lower_z(const FloorRegion& region,
                  const ElevationProvider& elevations, const Point_2& p) {
      return region.ceiling ? *region.ceiling : elevations.get(p);
    }

    // Avoid consecutive duplicate vertices while assembling a ring.
    void append(vec3f& ring, arr3f p) {
      if (ring.empty() || ring.back() != p) ring.push_back(p);
    }

    struct Candidate {
      Domain polygon;
      std::string input;
      float height;
      bool enabled = true;
    };

    // Find cutouts whose floor regions would collapse at the mesh tolerance.
    std::set<std::size_t> sub_tolerance_regions(
        Arrangement_2& floor,
        const std::unordered_map<Face_const_handle, FloorRegion>& regions,
        float tolerance) {
      const double tolerance_sq = double(tolerance) * tolerance;
      std::set<std::size_t> invalid_ceiling, invalid_ground;
      for (auto face : floor.face_handles()) {
        const auto& owner = regions.at(face);
        if (owner.part < 0) continue;
        std::vector<std::vector<Point_2>> rings;
        std::set<std::size_t> sources(owner.sources.begin(),
                                      owner.sources.end());
        auto ring = [&](auto first) {
          auto& points = rings.emplace_back();
          auto e = first;
          do {
            const auto& p = e->source()->point();
            // Detect collapses in the precision used by Mesh, too.
            points.emplace_back(float(CGAL::to_double(p.x())),
                                float(CGAL::to_double(p.y())));
            if (!owner.ceiling) {
              // Attribute a narrow ground gap to its neighbouring cutouts.
              const auto& values = regions.at(e->twin()->face()).sources;
              sources.insert(values.begin(), values.end());
            }
          } while (++e != first);
        };
        ring(face->outer_ccb());
        for (auto hole :
             CGAL::make_range(face->inner_ccbs_begin(), face->inner_ccbs_end()))
          ring(hole);
        if (sources.empty()) continue;
        // Check short edges and near contacts between non-adjacent boundaries.
        bool collapse = false;
        for (std::size_t a = 0; a < rings.size() && !collapse; ++a) {
          for (std::size_t i = 0; i < rings[a].size() && !collapse; ++i) {
            Segment_2 first(rings[a][i], rings[a][(i + 1) % rings[a].size()]);
            if (first.squared_length() <= tolerance_sq) {
              collapse = true;
              break;
            }
            for (std::size_t b = a; b < rings.size() && !collapse; ++b)
              for (std::size_t j = b == a ? i + 1 : 0; j < rings[b].size();
                   ++j) {
                if (a == b &&
                    (j == i + 1 || (i == 0 && j + 1 == rings[a].size())))
                  continue;
                Segment_2 second(rings[b][j],
                                 rings[b][(j + 1) % rings[b].size()]);
                if (CGAL::squared_distance(first, second) <= tolerance_sq) {
                  collapse = true;
                  break;
                }
              }
          }
        }
        if (collapse) {
          auto& invalid = owner.ceiling ? invalid_ceiling : invalid_ground;
          invalid.insert(sources.begin(), sources.end());
        }
      }
      // Remove a thin raised patch first, before considering the surrounding
      // ground region; this can retain another valid overlapping underpass.
      return invalid_ceiling.empty() ? invalid_ground : invalid_ceiling;
    }

    // Find cutouts that would create non-manifold vertical edges at vertices.
    std::set<std::size_t> invalid_junctions(
        Arrangement_2& floor,
        const std::unordered_map<Face_const_handle, FloorRegion>& regions,
        const ElevationProvider& elevations) {
      std::set<std::size_t> invalid;
      for (auto vertex : floor.vertex_handles()) {
        if (vertex->degree() < 4) continue;
        // Collect cyclic underside heights; exterior sectors contain no solid.
        std::vector<float> heights;
        std::vector<std::size_t> sources;
        auto first = vertex->incident_halfedges(), e = first;
        do {
          const auto& region = regions.at(e->face());
          heights.push_back(region.part < 0
                                ? std::numeric_limits<float>::infinity()
                                : lower_z(region, elevations, vertex->point()));
          sources.insert(sources.end(), region.sources.begin(),
                         region.sources.end());
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
          if (transitions > 2) invalid.insert(sources.begin(), sources.end());
        }
      }
      return invalid;
    }
  }  // namespace

  // Build the underside, resolve ceiling heights, and align it with the roof.
  void FloorArrangement::prepare(Arrangement_2& roof,
                                 const ElevationProvider& elevations,
                                 std::span<const Underpass> underpasses,
                                 const ArrangementExtruderConfig& cfg) {
    auto& log = logger::Logger::get_logger();
    // Copy only the final footprint boundary, retaining its facade chains.
    auto chains = boundary_chains(roof);
    std::vector<Segment_2> footprint;
    for (const auto& chain : chains)
      for (auto e : chain)
        footprint.emplace_back(e->source()->point(), e->target()->point());

    // Validate and snap each input independently; carving applies to LoD 2.2.
    std::vector<Candidate> candidates;
    if (cfg.lod2) {
      for (std::size_t i = 0; i < underpasses.size(); ++i) {
        const auto id = underpasses[i].source_id.empty()
                            ? std::to_string(i)
                            : underpasses[i].source_id;
        try {
          if (!std::isfinite(underpasses[i].ceiling_elevation))
            throw rooferException("non-finite ceiling elevation");
          for (const auto& p : underpasses[i].polygon)
            if (!std::isfinite(p[0]) || !std::isfinite(p[1]))
              throw rooferException("non-finite polygon coordinate");
          candidates.push_back(
              {snap_to_footprint(input_polygon(underpasses[i].polygon),
                                 footprint, cfg.snap_tolerance),
               id, float(underpasses[i].ceiling_elevation)});
        } catch (const std::exception& ex) {
          log.warning("Skipping underpass {}: {}", id, ex.what());
        }
      }
    }

    if (!candidates.empty()) {
      // Scratch overlay for clearance queries only. The actual roof receives
      // no underpass interior edges, including pillar boundaries.
      Arrangement_2 overlay = roof;
      for (const auto& candidate : candidates)
        insert_polygon(overlay, candidate.polygon);
      Location overlay_location(overlay), roof_location(roof);
      std::vector<float> minimum(candidates.size(),
                                 std::numeric_limits<float>::infinity());
      std::vector<float> maximum(candidates.size(),
                                 -std::numeric_limits<float>::infinity());
      // Accumulate roof minima and floor maxima over each covered interior
      // cell.
      for (auto face : overlay.face_handles()) {
        if (face->is_unbounded()) continue;
        auto sample = interior_point(face, overlay_location);
        auto roof_face = located_face(roof_location, sample);
        if (!roof_face->data().in_footprint) continue;
        std::vector<std::size_t> covered;
        for (std::size_t i = 0; i < candidates.size(); ++i)
          if (contains(candidates[i].polygon, sample)) covered.push_back(i);
        if (covered.empty()) continue;
        float min_z = std::numeric_limits<float>::infinity();
        visit_points(face, [&](const Point_2& p) {
          min_z = std::min(min_z, roof_z(roof_face, p, cfg));
        });
        auto max_z = elevations.get_max_elevation(face_domain(face));
        for (auto i : covered) {
          minimum[i] = std::min(minimum[i], min_z);
          maximum[i] = std::max(maximum[i], max_z);
        }
      }
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
          log.info("Lowering underpass {} ceiling from {} to {}", c.input,
                   c.height, limit);
          c.height = limit;
        }
        if (!std::isfinite(c.height) ||
            c.height <= maximum[i] + cfg.snap_tolerance) {
          c.enabled = false;
          log.warning(
              "Skipping underpass {}: no floor clearance below the roof",
              c.input);
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
            log.warning(
                "Skipping underpass {}: no floor clearance after height "
                "snapping",
                c.input);
          }
        }
      }
    }

    // Rebuild after rejecting cutouts that cause invalid or collapsing regions.
    for (;;) {
      floor.clear();
      regions.clear();
      for (const auto& segment : footprint) CGAL::insert(floor, segment);
      for (const auto& c : candidates)
        if (c.enabled) insert_polygon(floor, c.polygon);
      Location floor_location(floor), roof_location(roof);
      // Label interior regions; overlapping cutouts use the highest ceiling.
      for (auto face : floor.face_handles()) {
        FloorRegion region;
        if (!face->is_unbounded()) {
          auto sample = interior_point(face, floor_location);
          auto roof_face = located_face(roof_location, sample);
          if (roof_face->data().in_footprint) {
            region.part = roof_face->data().part_id;
            for (std::size_t i = 0; i < candidates.size(); ++i) {
              const auto& c = candidates[i];
              if (c.enabled && contains(c.polygon, sample)) {
                region.sources.push_back(i);
                region.ceiling =
                    std::max(region.ceiling.value_or(c.height), c.height);
              }
            }
          }
        }
        face->data().in_footprint = region.part >= 0;
        face->data().part_id = region.part;
        regions.emplace(face, std::move(region));
      }
      // Dissolve equal-height subdivisions while retaining their source IDs.
      for (auto it = floor.edges_begin(); it != floor.edges_end();) {
        Halfedge_handle edge = it;
        ++it;
        auto a = edge->face(), b = edge->twin()->face();
        auto ra = regions.at(a);
        const auto rb = regions.at(b);
        if (ra.part >= 0 && ra.part == rb.part && ra.ceiling == rb.ceiling) {
          ra.sources.insert(ra.sources.end(), rb.sources.begin(),
                            rb.sources.end());
          std::sort(ra.sources.begin(), ra.sources.end());
          ra.sources.erase(std::unique(ra.sources.begin(), ra.sources.end()),
                           ra.sources.end());
          auto merged = floor.remove_edge(edge);
          regions.erase(a);
          regions.erase(b);
          regions[merged] = ra;
        }
      }

      auto invalid = invalid_junctions(floor, regions, elevations);
      auto small = sub_tolerance_regions(floor, regions, cfg.snap_tolerance);
      invalid.insert(small.begin(), small.end());
      if (invalid.empty()) break;
      for (auto i : invalid) {
        candidates[i].enabled = false;
        log.warning(
            "Skipping underpass {}: non-manifold or sub-tolerance underside "
            "geometry",
            candidates[i].input);
      }
    }

    // Transfer facade IDs and synchronise floor and roof boundary vertices.
    EdgeObserver observer(roof);
    std::set<Point_2> points;
    for (auto e : floor.halfedge_handles())
      if (boundary(e)) {
        points.insert(e->source()->point());
        points.insert(e->target()->point());
        for (const auto& chain : chains) {
          Segment_2 line(chain.front()->source()->point(),
                         chain.back()->target()->point());
          if (line.has_on(e->source()->point()) &&
              line.has_on(e->target()->point())) {
            e->data().boundary_chain_id = e->twin()->data().boundary_chain_id =
                chain.front()->data().boundary_chain_id;
            break;
          }
        }
      }
    for (const auto& point : points) CGAL::insert_point(roof, point);
  }

  // Emit ground, ceilings, underside step walls, and complete facade polygons.
  void FloorArrangement::emit(
      Arrangement_2& roof, const ElevationProvider& elevations,
      const ArrangementExtruderConfig& cfg,
      const std::function<float(Vertex_handle, Face_handle)>& roof_height,
      std::unordered_map<int, Mesh>& meshes) const {
    // Reverse the arrangement winding so ground and ceiling normals point down.
    if (cfg.generate_floor) {
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
      LinearRing wall;
      append(wall, lifted(p, lower_z(a, elevations, p)));
      append(wall, lifted(q, lower_z(a, elevations, q)));
      append(wall, lifted(q, lower_z(b, elevations, q)));
      append(wall, lifted(p, lower_z(b, elevations, p)));
      meshes[a.part].push_polygon(wall, 3);
    }

    // Both arrangements retain the same footprint splits. Match by oriented
    // exact endpoints rather than by DCEL handles belonging to another graph.
    std::map<std::tuple<int, Point_2, Point_2>, Face_const_handle> floor_edges;
    for (auto e : floor.halfedge_handles())
      if (boundary(e))
        floor_edges[{e->data().boundary_chain_id, e->source()->point(),
                     e->target()->point()}] = e->face();
    // Trace each facade's floor profile forward and its roof profile backward.
    for (const auto& chain : boundary_chains(roof, false)) {
      vec3f bottom, top;
      for (auto e : chain) {
        auto p = e->source()->point(), q = e->target()->point();
        auto f = floor_edges.find({e->data().boundary_chain_id, p, q});
        if (f == floor_edges.end())
          throw rooferException("Roof/floor footprint boundary mismatch");
        const auto& region = regions.at(f->second);
        append(bottom, lifted(p, lower_z(region, elevations, p)));
        append(bottom, lifted(q, lower_z(region, elevations, q)));
        append(top, lifted(p, roof_height(e->source(), e->face())));
        append(top, lifted(q, roof_height(e->target(), e->face())));
      }
      LinearRing wall;
      wall.insert(wall.end(), bottom.begin(), bottom.end());
      for (auto it = top.rbegin(); it != top.rend(); ++it) append(wall, *it);
      if (wall.size() > 1 && wall.front() == wall.back()) wall.pop_back();
      if (wall.size() > 2)
        meshes[chain.front()->face()->data().part_id].push_polygon(wall, 2);
    }
  }

  // Split vertical edges at shared incident heights to remove T-junctions.
  void conform_vertical_edges(std::unordered_map<int, Mesh>& meshes) {
    for (auto& [part, mesh] : meshes) {
      // Collect all heights at each XY column within this solid.
      std::map<arr2f, std::set<float>> columns;
      auto visit = [&](auto&& callback) {
        for (auto& polygon : mesh.get_polygons()) {
          callback(polygon);
          for (auto& hole : polygon.interior_rings()) callback(hole);
        }
      };
      visit([&](const vec3f& ring) {
        for (const auto& p : ring) columns[{p[0], p[1]}].insert(p[2]);
      });
      // Insert intermediate heights in each vertical edge's traversal
      // direction.
      visit([&](vec3f& ring) {
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
}  // namespace roofer::reconstruction::detail
