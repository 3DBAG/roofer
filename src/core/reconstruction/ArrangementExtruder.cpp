// Copyright (c) 2018-2026 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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

#include <roofer/reconstruction/ArrangementBase.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include "FloorArrangement.hpp"

namespace roofer::reconstruction {

  // typedef CGAL::Cartesian<float>           AK;

  inline arr3f v2p(Arrangement_2::Vertex_handle v, float h) {
    return {float(CGAL::to_double(v->point().x())),
            float(CGAL::to_double(v->point().y())), h};
  }
  template <typename P>
  arr3f p2p(P p) {
    return {float(CGAL::to_double(p->x())), float(CGAL::to_double(p->y())),
            float(CGAL::to_double(p->z()))};
  }

  typedef std::pair<float, Face_handle> hf_pair;
  template <typename T>
  void push_ccb(
      T& ring, Halfedge_handle hedge,
      std::unordered_map<Vertex_handle, std::vector<hf_pair>>& vertex_columns,
      std::unordered_map<Halfedge_handle, EPECK::Point_3>& extra_wall_points) {
    auto first = hedge;
    do {
      auto v = hedge->source();
      if (CGAL::squared_distance(v->point(), hedge->target()->point()) > 0) {
        for (auto& [h, f_h] : vertex_columns[v]) {
          if (f_h == hedge->face()) {
            ring.push_back(v2p(v, h));
            break;
          }
        }
        auto p_xtra = extra_wall_points.find(hedge);
        auto q_xtra = extra_wall_points.find(hedge->twin());
        if (p_xtra != extra_wall_points.end()) {
          ring.push_back(p2p(&p_xtra->second));
        } else if (q_xtra != extra_wall_points.end()) {
          ring.push_back(p2p(&q_xtra->second));
        }
      }
      hedge = hedge->next();
    } while (hedge != first);
  }

  class ArrangementExtruder : public ArrangementExtruderInterface {
   public:
    void compute(Arrangement_2& arr,
                 const ElevationProvider& elevation_provider,
                 ArrangementExtruderConfig cfg) override {
      compute(arr, elevation_provider, std::span<const Underpass>{}, cfg);
    }

    void compute(Arrangement_2& arr,
                 const ElevationProvider& elevation_provider,
                 std::span<const Underpass> underpasses,
                 ArrangementExtruderConfig cfg) override {
      faces.clear();
      labels.clear();
      meshes.clear();
      multisolid.clear();
      diagnostics.clear();
      if (auto error = config::validate(cfg)) throw rooferException(*error);
      for (auto face : arr.face_handles())
        if (face->data().in_footprint && face->data().part_id < 0)
          face->data().part_id = 0;
      float snap_tolerance = cfg.snap_tolerance;
      float snap_tolerance_sq = snap_tolerance * snap_tolerance;

      auto unbounded_face = arr.unbounded_face();
      unbounded_face->data().elevation_70p =
          elevation_provider.get_percentile(0.7);
      unbounded_face->data().elevation_97p =
          elevation_provider.get_percentile(0.97);

      detail::FloorArrangement underside;
      detail::WallAssembler walls;

      // check for no detected planes (if one face inside fp has none then all
      // have none)
      for (const auto f : arr.face_handles()) {
        if (f->data().in_footprint && f->data().segid == 0) {
          auto floor_config = cfg;
          floor_config.generate_walls = false;
          underside.emit(
              arr, elevation_provider, floor_config,
              [](Vertex_handle, Face_handle) { return 0.0F; }, multisolid,
              walls);
          detail::remove_redundant_vertices(multisolid, cfg.snap_tolerance);
          collect_outputs();
          return;
        }
      }

      underside.prepare(arr, elevation_provider, underpasses, cfg, diagnostics);

      // compute all heights for each vertex
      std::unordered_map<Vertex_handle, std::vector<hf_pair>> vertex_columns;
      for (auto& v : arr.vertex_handles()) {
        auto& p = v->point();
        auto he = v->incident_halfedges();
        auto first = he;
        std::vector<hf_pair> heights;
        do {
          auto f = he->face();
          float h;
          if (f->data().in_footprint && f->data().segid != 0) {
            if (cfg.lod2) {
              auto& plane = f->data().plane;
              h = (plane.a() * CGAL::to_double(p.x()) +
                   plane.b() * CGAL::to_double(p.y()) + plane.d()) /
                  (-plane.c());
            } else {
              if (cfg.lod1_extrude_to_max)
                h = f->data().elevation_97p;
              else
                h = f->data().elevation_70p;
            }
          } else if (f->data().in_footprint) {
            h = cfg.nodata_elevation;
          } else {
            h = elevation_provider.get(p);
          }
          heights.push_back(std::make_pair(h, f));
        } while (++he != first);

        if (heights.size() == 0) continue;

        // sort heights
        std::sort(heights.begin(), heights.end(),
                  [](hf_pair& a, hf_pair& b) { return a.first < b.first; });
        // equalise heights that are practically the same
        float h_ref = heights[0].first;
        for (auto& [h, face] : heights) {
          if (std::fabs(h_ref - h) < snap_tolerance) {
            h = h_ref;
          }
          h_ref = h;
        }

        vertex_columns[v] = heights;
      }

      const auto roof_height = [&](Vertex_handle vertex, Face_handle face) {
        for (const auto& [height, candidate] : vertex_columns.at(vertex))
          if (candidate == face) return height;
        throw rooferException("Missing roof vertex height");
      };

      // Roof profiles may cross along a step. Keep that intersection in both
      // roof rings; intermediate vertical heights are inserted once, at the
      // end.
      std::unordered_map<Halfedge_handle, EPECK::Point_3> extra_wall_points;
      if (cfg.generate_walls) {
        for (auto edge : arr.edge_handles()) {
          auto a = edge->twin()->face(), b = edge->face();
          if (!a->data().in_footprint || !b->data().in_footprint) continue;
          auto v1 = edge->source(), v2 = edge->target();
          const auto& p1 = v1->point();
          const auto& p2 = v2->point();
          if (CGAL::squared_distance(p1, p2) < snap_tolerance_sq) continue;
          const float h1a = roof_height(v1, a), h1b = roof_height(v1, b);
          const float h2a = roof_height(v2, a), h2b = roof_height(v2, b);
          auto add_step = [&](const Point_2& p, const Point_2& q,
                              std::array<float, 2> ha,
                              std::array<float, 2> hb) {
            // A roof step has solid on the side with the higher roof.
            if (ha[0] > hb[0] || ha[1] > hb[1])
              walls.add(q, p, {hb[1], hb[0]}, {ha[1], ha[0]}, b->data().part_id,
                        3);
            else
              walls.add(p, q, ha, hb, b->data().part_id, 3);
          };
          if ((h1a < h1b && h2a > h2b) || (h1a > h1b && h2a < h2b)) {
            const auto line_a =
                EPECK::Line_3(EPECK::Point_3(p1.x(), p1.y(), h1a),
                              EPECK::Point_3(p2.x(), p2.y(), h2a));
            const auto line_b =
                EPECK::Line_3(EPECK::Point_3(p1.x(), p1.y(), h1b),
                              EPECK::Point_3(p2.x(), p2.y(), h2b));
            const auto intersection = CGAL::intersection(line_a, line_b);
            const auto* point =
                intersection ? std::get_if<EPECK::Point_3>(&*intersection)
                             : nullptr;
            if (!point)
              throw rooferException("Invalid intersection point in Extruder");
            extra_wall_points[edge] = *point;
            const Point_2 crossing(point->x(), point->y());
            const float z = float(CGAL::to_double(point->z()));
            add_step(p1, crossing, {h1a, z}, {h1b, z});
            add_step(crossing, p2, {z, h2a}, {z, h2b});
          } else {
            add_step(p1, p2, {h1a, h2a}, {h1b, h2b});
          }
        }
      }

      // roofs
      if (cfg.generate_roofs) {
        for (auto face : arr.face_handles()) {
          if (face->data().in_footprint) {
            LinearRing roofpart;
            auto he = face->outer_ccb();

            push_ccb(roofpart, he, vertex_columns, extra_wall_points);

            for (Arrangement_2::Hole_iterator hole = face->holes_begin();
                 hole != face->holes_end(); ++hole) {
              vec3f roofpart_hole;
              auto he = *hole;
              push_ccb(roofpart_hole, he, vertex_columns, extra_wall_points);
              if (roofpart_hole.size() > 2) {
                roofpart.interior_rings().push_back(roofpart_hole);
              }
            }

            if (roofpart.size() > 2) {
              multisolid[face->data().part_id].push_polygon(roofpart, int(1));
              // mesh.push_attribute("surface_type", int(1));
            }
          }
        }
      }

      underside.emit(arr, elevation_provider, cfg, roof_height, multisolid,
                     walls);
      walls.emit(multisolid);
      detail::conform_vertical_edges(multisolid);
      detail::remove_redundant_vertices(multisolid, cfg.snap_tolerance);
      collect_outputs();
    }

    void collect_outputs() {
      for (auto& [part, mesh] : multisolid) {
        faces.insert(faces.end(), mesh.get_polygons().begin(),
                     mesh.get_polygons().end());
        labels.insert(labels.end(), mesh.get_labels().begin(),
                      mesh.get_labels().end());
        meshes.push_back(mesh);
      }
    }

    /*
     * Convenience overload when base_elevation is passed
     */
    void compute(Arrangement_2& arr, float base_elevation,
                 ArrangementExtruderConfig cfg) override {
      auto const_elevation_provider_ptr =
          createElevationProvider(base_elevation);
      compute(arr, *const_elevation_provider_ptr, cfg);
    }
  };

  std::unique_ptr<ArrangementExtruderInterface> createArrangementExtruder() {
    return std::make_unique<ArrangementExtruder>();
  }
}  // namespace roofer::reconstruction
