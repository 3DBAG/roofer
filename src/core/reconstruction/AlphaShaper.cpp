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

#include <roofer/reconstruction/AlphaShaper.hpp>
#include "TriangulationBoundary.hpp"
#include <algorithm>

// 2D alpha shapes
#include <CGAL/Alpha_shape_2.h>
#include <CGAL/Alpha_shape_face_base_2.h>
#include <CGAL/Alpha_shape_vertex_base_2.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Projection_traits_xy_3.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>

namespace roofer::reconstruction {
  static const int EXTERIOR = -1, NEVER_VISITED = -2, HOLE = -3;

  typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
  typedef CGAL::Projection_traits_xy_3<K> Gt;
  typedef K::FT FT;

  typedef CGAL::Alpha_shape_vertex_base_2<Gt> Vb;
  struct FaceInfo {
    int label = NEVER_VISITED;
  };
  typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo, K> Fbb;
  typedef CGAL::Alpha_shape_face_base_2<Gt, Fbb> Fb;

  typedef CGAL::Triangulation_data_structure_2<Vb, Fb> Tds;
  typedef CGAL::Delaunay_triangulation_2<Gt, Tds> Triangulation_2;
  typedef CGAL::Alpha_shape_2<Triangulation_2> Alpha_shape_2;
  typedef Alpha_shape_2::Vertex_handle Vertex_handle;
  typedef Alpha_shape_2::Edge Edge;
  typedef Alpha_shape_2::Face_handle Face_handle;
  typedef Alpha_shape_2::Vertex_handle Vertex_handle;
  typedef Alpha_shape_2::Vertex_circulator Vertex_circulator;
  typedef Alpha_shape_2::Edge_circulator Edge_circulator;

  class AlphaShapeRegionGrower {
    Alpha_shape_2& A;
    enum Mode {
      LABEL_INFINITE_FACE,  // stop at alpha boundary
      LABEL_INTERIOR_FACE,  // stop at faces labels as exterior
      LABEL_HOLE_FACE
    };
    int label_cnt = 0;  // label==-1 means exterior, -2 mean never visited, -3
                        // means a hole, 0+ means a regular region
    int hole_cnt = HOLE;

   public:
    AlphaShapeRegionGrower(Alpha_shape_2& as) : A(as){};

    void grow() {
      std::stack<Face_handle> interior_seeds, hole_seeds;
      // label triangles reachable from inf face as -1; ie exterior
      auto inf_face = A.infinite_face();
      inf_face->info().label = EXTERIOR;
      grow_region(
          inf_face,
          LABEL_INFINITE_FACE);  // this sets label of exterior faces to -1
      // label remaining faces that are not interior in the a-shape as -3, and
      // push faces that are inside the a-shape on the seeds stack
      for (auto fh = A.all_faces_begin(); fh != A.all_faces_end(); ++fh) {
        if (fh->info().label != EXTERIOR) {
          if (A.classify(fh) == Alpha_shape_2::INTERIOR) {
            interior_seeds.push(fh);
          } else {
            hole_seeds.push(fh);
            // fh->info().label = HOLE;
          }
        }
      };
      while (!hole_seeds.empty()) {
        auto fh = hole_seeds.top();
        hole_seeds.pop();
        if (fh->info().label == NEVER_VISITED) {
          fh->info().label = hole_cnt;
          grow_region(fh, LABEL_HOLE_FACE);
          --hole_cnt;
        }
      }
      while (!interior_seeds.empty()) {
        auto fh = interior_seeds.top();
        interior_seeds.pop();
        if (fh->info().label == NEVER_VISITED) {
          fh->info().label = label_cnt;
          grow_region(fh, LABEL_INTERIOR_FACE);
          ++label_cnt;
        }
      }
    }

    void grow_region(Face_handle face_handle, Mode mode) {
      std::stack<Face_handle> candidates;
      candidates.push(face_handle);

      while (candidates.size() > 0) {
        auto fh = candidates.top();
        candidates.pop();
        // check the 3 neighbors of this face
        for (int i = 0; i < 3; ++i) {
          auto e = std::make_pair(fh, i);
          auto neighbor = fh->neighbor(i);

          // check if neighbor handle is not pointing anywhere. Should not
          // happen, but it does occasionally in practice.
          if (neighbor == nullptr) {
            // std::cout << "null\n" << std::flush;
            continue;
          }

          if (mode == LABEL_INFINITE_FACE) {
            // add neighbor if it is not on the ohter side of alpha boundary
            // check if this neighbor hasn't been visited before
            if (neighbor->info().label == NEVER_VISITED) {
              auto edge_class = A.classify(e);
              if (!(edge_class == Alpha_shape_2::REGULAR)) {
                neighbor->info().label = EXTERIOR;
                candidates.push(neighbor);
              }
            }
          } else if (mode == LABEL_HOLE_FACE) {
            if (neighbor->info().label == NEVER_VISITED) {
              auto edge_class = A.classify(e);
              if (!(edge_class == Alpha_shape_2::REGULAR)) {
                neighbor->info().label = hole_cnt;
                candidates.push(neighbor);
              }
            }
          } else if (mode == LABEL_INTERIOR_FACE) {
            // check if this neighbor hasn't been visited before and is not
            // exterior/hole
            if (neighbor->info().label == NEVER_VISITED) {
              neighbor->info().label = label_cnt;
              candidates.push(neighbor);
            }
          }
        }
      }
    }
  };

  class AlphaShaper : public AlphaShaperInterface {
    void compute(const IndexedPlanesWithPoints& pts_per_roofplane,
                 AlphaShaperConfig cfg) override {
      alpha_rings.clear();
      alpha_triangles.clear();
      roofplane_ids.clear();

      PointCollection edge_points;
      LineStringCollection alpha_edges;

      std::vector<Triangulation_2> alpha_dts;
      vec1i segment_ids;
      for (auto& it : pts_per_roofplane) {
        if (it.first == -1)
          continue;  // skip points if they put at index -1 (eg if we care not
                     // about slanted surfaces for ring extraction)
        auto points = it.second.second;
        if (points.size() < 3) continue;
        Triangulation_2 T;
        T.insert(points.begin(), points.end());
        if (T.dimension() < 2) continue;
        Alpha_shape_2 A(T, FT(cfg.alpha), Alpha_shape_2::GENERAL);

        double alpha = cfg.alpha;
        if (cfg.optimal_alpha && cfg.clamp_optimal_alpha) {
          alpha = std::max(float(*A.find_optimal_alpha(1)), cfg.alpha);
        } else if (cfg.optimal_alpha) {
          alpha = *A.find_optimal_alpha(1);
        }
        A.set_alpha(FT(alpha));

        for (auto it = A.alpha_shape_vertices_begin();
             it != A.alpha_shape_vertices_end(); it++) {
          auto p = (*it)->point();
          edge_points.push_back({float(p.x()), float(p.y()), float(p.z())});
        }
        for (auto it = A.alpha_shape_edges_begin();
             it != A.alpha_shape_edges_end(); it++) {
          auto p1 = it->first->vertex(A.cw(it->second))->point();
          auto p2 = it->first->vertex(A.ccw(it->second))->point();

          alpha_edges.push_back(
              {{float(p1.x()), float(p1.y()), float(p1.z())},
               {float(p2.x()), float(p2.y()), float(p2.z())}});
        }

        // flood filling
        auto grower = AlphaShapeRegionGrower(A);
        grower.grow();

        // collect triangles
        for (auto fh = A.finite_faces_begin(); fh != A.finite_faces_end();
             ++fh) {
          // only export triangles in the interior of a shape (thus excluding
          // holes and exterior)
          if (fh->info().label >= 0) {
            arr3f p0 = {float(fh->vertex(0)->point().x()),
                        float(fh->vertex(0)->point().y()),
                        float(fh->vertex(0)->point().z())};
            arr3f p1 = {float(fh->vertex(1)->point().x()),
                        float(fh->vertex(1)->point().y()),
                        float(fh->vertex(1)->point().z())};
            arr3f p2 = {float(fh->vertex(2)->point().x()),
                        float(fh->vertex(2)->point().y()),
                        float(fh->vertex(2)->point().z())};
            alpha_triangles.push_back({p0, p1, p2});
            segment_ids.push_back(fh->info().label);
            segment_ids.push_back(fh->info().label);
            segment_ids.push_back(fh->info().label);
          }
        }

        if (cfg.extract_polygons) {
          auto polygons = detail::triangulation_polygons(
              A, [](Face_handle face) { return face->info().label; },
              [](Vertex_handle vertex) {
                const auto& p = vertex->point();
                return arr3f{float(p.x()), float(p.y()), float(p.z())};
              });
          // Canonicalize each ring in linear time, preserving winding.
          // Coordinate ordering also makes holes and components independent of
          // boundary seeds and triangulation handle order within this roof
          // plane.
          auto canonicalize = [](vec3f& ring) {
            std::rotate(ring.begin(),
                        std::min_element(ring.begin(), ring.end()), ring.end());
          };
          for (auto& polygon : polygons) {
            canonicalize(polygon.exterior);
            for (auto& hole : polygon.holes) canonicalize(hole);
            std::sort(polygon.holes.begin(), polygon.holes.end());
          }
          std::sort(polygons.begin(), polygons.end(),
                    [](const auto& a, const auto& b) {
                      return a.exterior < b.exterior;
                    });
          for (auto& polygon : polygons) {
            LinearRing ring;
            ring.insert(ring.end(), polygon.exterior.begin(),
                        polygon.exterior.end());
            ring.interior_rings() = std::move(polygon.holes);
            ring.push_back(ring.front());
            for (auto& hole : ring.interior_rings())
              hole.push_back(hole.front());
            alpha_rings.push_back(std::move(ring));
            roofplane_ids.push_back(it.first);
          }
        }
      }

      // output("alpha_edges").set(alpha_edges);
      // output("segment_ids").set(segment_ids);
      // output("edge_points").set(edge_points);
    }
  };

  std::unique_ptr<AlphaShaperInterface> createAlphaShaper() {
    return std::make_unique<AlphaShaper>();
  };

}  // namespace roofer::reconstruction
