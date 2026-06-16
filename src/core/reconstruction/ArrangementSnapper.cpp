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
// Ravi Peters

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

#include <roofer/reconstruction/ArrangementBase.hpp>
#include <roofer/reconstruction/ArrangementSnapper.hpp>
#include <roofer/reconstruction/cdt_util.hpp>
// #include <CGAL/Constrained_triangulation_2.h>
#include <CGAL/Arr_walk_along_line_point_location.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>

#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_set>

namespace roofer::reconstruction {

  namespace arragementsnapper {

    typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
    typedef CGAL::Exact_predicates_inexact_constructions_kernel Epeck;
    typedef CGAL::Exact_predicates_tag Tag;

    typedef CGAL::Triangulation_vertex_base_2<K> VertexBase;
    typedef CGAL::Triangulation_vertex_base_with_info_2<bool, K, VertexBase>
        VertexBaseWithInfo;
    typedef CGAL::Constrained_triangulation_face_base_2<K> FaceBase;
    typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo*, K, FaceBase>
        FaceBaseWithInfo;
    typedef CGAL::Triangulation_data_structure_2<VertexBaseWithInfo,
                                                 FaceBaseWithInfo>
        TriangulationDataStructure;
    typedef CGAL::Constrained_Delaunay_triangulation_2<
        K, TriangulationDataStructure, Tag>
        T;
    typedef T::Edge_circulator Edge_circulator;
    typedef T::Face_circulator Face_circulator;
    typedef T::Finite_faces_iterator Finite_faces_iterator;
    typedef T::Finite_edges_iterator Finite_edges_iterator;
    typedef T::Vertex_handle Vertex_handle;
    typedef T::Face_handle Face_handle;
    typedef std::pair<Face_handle, int> Edge;

    typedef std::unordered_set<Vertex_handle> ConstraintSet;

    struct ForcedRegionLabel {
      T::Point_2 seed;
      FaceInfo* face_info;
    };

    using SourceFaceIds = std::unordered_map<FaceInfo*, std::size_t>;

    struct IncidentSector {
      Vertex_handle neighbour;
      FaceInfo* face_info;
      double angle;
      double height;
    };

    struct RepairCandidate {
      Vertex_handle vertex;
      std::vector<IncidentSector> sectors;
      double clearance;
    };

    double plane_height(const FaceInfo& face_info, const T::Point_2& point) {
      const auto& plane = face_info.plane;
      if (plane.c() == 0) return 0;
      return -(plane.a() * point.x() + plane.b() * point.y() + plane.d()) /
             plane.c();
    }

    FaceInfo* source_face_at(
        const T::Point_2& point,
        CGAL::Arr_walk_along_line_point_location<Arrangement_2>& walk_pl,
        Arrangement_2& source_arrangement) {
      auto object =
          walk_pl.locate(Arrangement_2::Point_2(point.x(), point.y()));
      if (auto face = std::get_if<Face_const_handle>(&object)) {
        return &source_arrangement.non_const_handle(*face)->data();
      }
      return nullptr;
    }

    std::vector<std::vector<Face_handle>> constrained_regions(T& tri) {
      std::vector<std::vector<Face_handle>> regions;
      std::unordered_set<Face_handle> visited;

      for (auto face : tri.finite_face_handles()) {
        if (visited.contains(face)) continue;

        std::vector<Face_handle> region;
        std::queue<Face_handle> queue;
        queue.push(face);
        visited.insert(face);
        while (!queue.empty()) {
          auto current = queue.front();
          queue.pop();
          region.push_back(current);

          for (int i = 0; i < 3; ++i) {
            Edge edge(current, i);
            auto neighbour = current->neighbor(i);
            if (!tri.is_constrained(edge) && !tri.is_infinite(neighbour) &&
                visited.insert(neighbour).second) {
              queue.push(neighbour);
            }
          }
        }
        regions.push_back(std::move(region));
      }
      return regions;
    }

    void label_final_regions(
        T& tri,
        CGAL::Arr_walk_along_line_point_location<Arrangement_2>& walk_pl,
        Arrangement_2& source_arrangement, const SourceFaceIds& source_ids,
        const std::vector<ForcedRegionLabel>& forced_labels) {
      std::vector<std::pair<Face_handle, FaceInfo*>> forced_face_labels;
      for (const auto& forced : forced_labels) {
        auto located = tri.locate(forced.seed);
        if (!tri.is_infinite(located)) {
          forced_face_labels.emplace_back(located, forced.face_info);
        }
      }

      for (auto& region : constrained_regions(tri)) {
        std::unordered_map<FaceInfo*, double> overlap_area;
        for (auto face : region) {
          auto centroid = CGAL::centroid(tri.triangle(face));
          if (auto* source =
                  source_face_at(centroid, walk_pl, source_arrangement)) {
            overlap_area[source] += std::abs(tri.triangle(face).area());
          }
        }

        FaceInfo* selected = nullptr;
        double selected_area = -1;
        std::size_t selected_id = std::numeric_limits<std::size_t>::max();
        for (const auto& [source, area] : overlap_area) {
          auto id = source_ids.at(source);
          if (area > selected_area ||
              (area == selected_area && id < selected_id)) {
            selected = source;
            selected_area = area;
            selected_id = id;
          }
        }

        for (const auto& [forced_face, forced_label] : forced_face_labels) {
          if (std::find(region.begin(), region.end(), forced_face) !=
              region.end()) {
            selected = forced_label;
            break;
          }
        }

        for (auto face : region) face->info() = selected;
      }
    }

    std::optional<double> vertex_clearance(T& tri, Vertex_handle vertex) {
      double clearance_sq = std::numeric_limits<double>::max();
      Face_circulator face = tri.incident_faces(vertex), done(face);
      if (face == nullptr) return std::nullopt;
      do {
        if (tri.is_infinite(face)) return std::nullopt;
        auto opposite_edge = Edge(face, face->index(vertex));
        clearance_sq = std::min(
            clearance_sq, CGAL::to_double(CGAL::squared_distance(
                              vertex->point(), tri.segment(opposite_edge))));
      } while (++face != done);
      if (!(clearance_sq > 0)) return std::nullopt;
      return std::sqrt(clearance_sq);
    }

    std::size_t constrained_incident_edge_count(T& tri, Vertex_handle vertex) {
      std::size_t count = 0;
      Edge_circulator edge = tri.incident_edges(vertex), done(edge);
      if (edge == nullptr) return count;
      do {
        if (tri.is_constrained(*edge)) ++count;
      } while (++edge != done);
      return count;
    }

    std::vector<IncidentSector> incident_sectors(T& tri, Vertex_handle vertex,
                                                 double clearance) {
      std::vector<IncidentSector> sectors;
      Edge_circulator edge = tri.incident_edges(vertex), done(edge);
      if (edge == nullptr) return sectors;
      do {
        if (!tri.is_constrained(*edge)) continue;
        auto first = edge->first->vertex(tri.cw(edge->second));
        auto second = edge->first->vertex(tri.ccw(edge->second));
        auto neighbour = first == vertex ? second : first;
        const double dx = neighbour->point().x() - vertex->point().x();
        const double dy = neighbour->point().y() - vertex->point().y();
        sectors.push_back({neighbour, nullptr, std::atan2(dy, dx), 0});
      } while (++edge != done);

      std::sort(sectors.begin(), sectors.end(),
                [](const auto& lhs, const auto& rhs) {
                  return lhs.angle < rhs.angle;
                });
      if (sectors.size() < 2) return sectors;

      const double sample_radius = clearance * 0.25;
      constexpr double two_pi = 2 * CGAL_PI;
      for (std::size_t i = 0; i < sectors.size(); ++i) {
        double next_angle = sectors[(i + 1) % sectors.size()].angle;
        if (next_angle <= sectors[i].angle) next_angle += two_pi;
        const double sample_angle = (sectors[i].angle + next_angle) * 0.5;
        T::Point_2 sample(
            vertex->point().x() + sample_radius * std::cos(sample_angle),
            vertex->point().y() + sample_radius * std::sin(sample_angle));
        auto face = tri.locate(sample);
        if (tri.is_infinite(face) || face->info() == nullptr) return {};
        sectors[i].face_info = face->info();
        sectors[i].height = plane_height(*face->info(), vertex->point());
      }
      return sectors;
    }

    bool has_non_manifold_height_order(
        const std::vector<IncidentSector>& sectors, double tolerance) {
      std::vector<double> heights;
      heights.reserve(sectors.size());
      for (const auto& sector : sectors) heights.push_back(sector.height);
      std::sort(heights.begin(), heights.end());

      std::vector<double> distinct_heights;
      for (double height : heights) {
        if (distinct_heights.empty() ||
            height - distinct_heights.back() >= tolerance) {
          distinct_heights.push_back(height);
        }
      }

      for (std::size_t level = 1; level < distinct_heights.size(); ++level) {
        const double sample_height =
            (distinct_heights[level - 1] + distinct_heights[level]) * 0.5;
        std::size_t crossings = 0;
        for (std::size_t i = 0; i < sectors.size(); ++i) {
          const double lhs =
              sectors[(i + sectors.size() - 1) % sectors.size()].height;
          const double rhs = sectors[i].height;
          if ((lhs < sample_height && rhs > sample_height) ||
              (rhs < sample_height && lhs > sample_height)) {
            ++crossings;
          }
        }
        if (crossings > 2) return true;
      }
      return false;
    }

    std::optional<RepairCandidate> find_problematic_vertex(
        T& tri, double height_tolerance) {
      for (auto vertex = tri.finite_vertices_begin();
           vertex != tri.finite_vertices_end(); ++vertex) {
        if (vertex->info()) continue;
        if (constrained_incident_edge_count(tri, vertex) < 4) continue;
        auto clearance = vertex_clearance(tri, vertex);
        if (!clearance) continue;
        auto sectors = incident_sectors(tri, vertex, *clearance);
        if (sectors.size() < 4) continue;
        if (std::any_of(sectors.begin(), sectors.end(), [](const auto& sector) {
              return !sector.face_info->in_footprint ||
                     sector.face_info->segid == 0;
            })) {
          continue;
        }
        if (has_non_manifold_height_order(sectors, height_tolerance)) {
          return RepairCandidate{vertex, std::move(sectors), *clearance};
        }
      }
      return std::nullopt;
    }

    ForcedRegionLabel repair_vertex(T& tri, const RepairCandidate& candidate,
                                    const SourceFaceIds& source_ids,
                                    double maximum_radius,
                                    double height_tolerance) {
      const auto& sectors = candidate.sectors;
      auto vertex = candidate.vertex;
      if (sectors.size() < 4) {
        throw roofer::rooferException(
            "Unable to compute a safe non-manifold junction repair");
      }

      const double min_height =
          std::min_element(sectors.begin(), sectors.end(),
                           [](const auto& lhs, const auto& rhs) {
                             return lhs.height < rhs.height;
                           })
              ->height;
      FaceInfo* selected_face = nullptr;
      std::size_t selected_id = std::numeric_limits<std::size_t>::max();
      for (const auto& sector : sectors) {
        if (sector.height <= min_height + height_tolerance) {
          auto id = source_ids.at(sector.face_info);
          if (id < selected_id) {
            selected_face = sector.face_info;
            selected_id = id;
          }
        }
      }

      double shortest_edge = std::numeric_limits<double>::max();
      for (const auto& sector : sectors) {
        shortest_edge = std::min(
            shortest_edge, std::sqrt(CGAL::to_double(CGAL::squared_distance(
                               vertex->point(), sector.neighbour->point()))));
      }
      const double radius = std::min(
          {maximum_radius, candidate.clearance * 0.8, shortest_edge * 0.45});
      if (!(radius > std::numeric_limits<double>::epsilon())) {
        throw roofer::rooferException(
            "Non-manifold junction repair radius is degenerate");
      }

      const auto original_point = vertex->point();
      std::vector<Vertex_handle> split_vertices;
      split_vertices.reserve(sectors.size());
      for (const auto& sector : sectors) {
        const double dx = sector.neighbour->point().x() - original_point.x();
        const double dy = sector.neighbour->point().y() - original_point.y();
        const double length = std::sqrt(dx * dx + dy * dy);
        T::Point_2 split_point(original_point.x() + radius * dx / length,
                               original_point.y() + radius * dy / length);
        auto split_vertex = tri.insert(split_point);
        split_vertex->info() = false;
        split_vertices.push_back(split_vertex);
      }

      tri.remove_incident_constraints(vertex);
      tri.remove(vertex);
      for (std::size_t i = 0; i < sectors.size(); ++i) {
        tri.insert_constraint(split_vertices[i], sectors[i].neighbour);
      }

      std::optional<T::Point_2> forced_seed;
      for (std::size_t i = 0; i < sectors.size(); ++i) {
        auto next = (i + 1) % sectors.size();
        if (sectors[i].face_info == selected_face) {
          if (!forced_seed) {
            forced_seed = T::Point_2(
                (2 * original_point.x() + split_vertices[i]->point().x() +
                 split_vertices[next]->point().x()) /
                    4,
                (2 * original_point.y() + split_vertices[i]->point().y() +
                 split_vertices[next]->point().y()) /
                    4);
          }
          continue;
        }
        tri.insert_constraint(split_vertices[i], split_vertices[next]);
      }

      if (!forced_seed) {
        throw roofer::rooferException(
            "Non-manifold junction repair has no merge sector");
      }
      return {*forced_seed, selected_face};
    }

    // tri_util::CDT triangulate_polygon(LinearRing& poly, float
    // dupe_threshold_exp=3) {
    //   tri_util::CDT triangulation;

    //   float dupe_threshold = (float) std::pow(10,-dupe_threshold_exp);

    //   tri_util::insert_ring(poly, triangulation);
    //   for (auto& ring : poly.interior_rings()) {
    //     tri_util::insert_ring(ring, triangulation);
    //   }

    //   if (triangulation.number_of_faces()==0)
    //     return triangulation;

    //   mark_domains(triangulation);

    //   return triangulation;
    // }

    void get_incident_constraints(T& tri, Vertex_handle& vthis,
                                  Vertex_handle& vexcept1,
                                  Vertex_handle vexcept2,
                                  ConstraintSet& constraints_to_restore) {
      // std::cout << "vthis degree=" << tri.degree(vthis) << std::endl;
      Edge_circulator ec = tri.incident_edges(vthis), done(ec);
      if (ec != nullptr) {
        do {
          if (tri.is_constrained(*ec)) {
            auto va = ec->first->vertex(tri.cw(ec->second));
            auto vb = ec->first->vertex(tri.ccw(ec->second));
            Vertex_handle vother;
            if (va == vthis) {
              vother = vb;
            } else {
              vother = va;
            };

            if (vother != vexcept1 && vother != vexcept2) {
              constraints_to_restore.insert(vother);
            }
          }
        } while (++ec != done);
      }
    }

    void restore_constraints(T& tri, T::Point_2& pnew,
                             ConstraintSet& constraints_to_restore) {
      auto vnew = tri.insert(pnew);

      // restore constraints
      // std::cout << "restoring " << constraints_to_restore.size() << "
      // constraints\n";
      for (auto& vh : constraints_to_restore) {
        // std::cout << "reinsert constrained " << *vnew << " - " << *vh <<
        // std::endl;
        tri.insert_constraint(vnew, vh);
      }
    }

    class ArrangementSnapper : public ArrangementSnapperInterface {
      void compute(Arrangement_2& arr, ArrangementSnapperConfig cfg) override {
        typedef CGAL::Arr_walk_along_line_point_location<Arrangement_2> Walk_pl;

        T tri;
        float sq_dist_thres = cfg.dist_thres * cfg.dist_thres;

        SourceFaceIds source_face_ids;
        std::size_t next_source_face_id = 0;
        for (auto face : arr.face_handles()) {
          source_face_ids[&face->data()] = next_source_face_id++;
        }

        // map from arr vertices to tri vertices
        std::unordered_map<Arrangement_2::Vertex_handle, T::Vertex_handle>
            vertex_map;

        // Segment_list_2 seg_list;
        // Polyline_list_2 output_list;
        for (auto arrVertex : arr.vertex_handles()) {
          // auto& p = v->point();

          // check if this vertex is on the footprint
          Arrangement_2::Halfedge_around_vertex_circulator
              ec = arrVertex->incident_halfedges(),
              done(ec);
          bool has_ext_face(false), has_int_face(false);
          if (ec != nullptr) {
            do {
              has_ext_face = !ec->face()->data().in_footprint || has_ext_face;
              has_int_face = ec->face()->data().in_footprint || has_int_face;
            } while (++ec != done);
          }

          auto vtri =
              tri.insert(T::Point_2(CGAL::to_double(arrVertex->point().x()),
                                    CGAL::to_double(arrVertex->point().y())));
          vtri->info() = has_ext_face && has_int_face;
          vertex_map[arrVertex] = vtri;
        }

        Walk_pl walk_pl(arr);
        for (auto& arrEdge : arr.edge_handles()) {
          if (vertex_map[arrEdge->source()] != vertex_map[arrEdge->target()]) {
            tri.insert_constraint(vertex_map[arrEdge->source()],
                                  vertex_map[arrEdge->target()]);
          }
        }

        // remove isolated vertices (sometimes these result from input
        // arrangements with edge between 2 vertices with the same coordinates)
        // {
        //   std::vector<T::Vertex_handle> to_remove;
        //   for (auto vh = tri.finite_vertices_begin(); vh !=
        //   tri.finite_vertices_end(); ++vh) {
        //     if (!tri.are_there_incident_constraints(vh)) {
        //       to_remove.push_back(vh);
        //     }
        //   }
        //   for (auto& v: to_remove) {
        //     std:: cout << "removing vertex without incident constraints...\n"
        //     tri.remove(v);
        //   }
        // }

        // TriangleCollection triangles_og;
        // vec1i segment_ids_og;
        // for (auto fh = tri.finite_faces_begin(); fh !=
        // tri.finite_faces_end(); ++fh) {
        //   // only export triangles in the interior of a shape (thus excluding
        //   holes and exterior)

        //     arr3f p0 = {float (fh->vertex(0)->point().x()), float
        //     (fh->vertex(0)->point().y()), 0}; arr3f p1 = {float
        //     (fh->vertex(1)->point().x()), float (fh->vertex(1)->point().y()),
        //     0}; arr3f p2 = {float (fh->vertex(2)->point().x()), float
        //     (fh->vertex(2)->point().y()), 0}; triangles_og.push_back({
        //     p0,p1,p2 }); segment_ids_og.push_back(fh->info()->segid);
        //     segment_ids_og.push_back(fh->info()->segid);
        //     segment_ids_og.push_back(fh->info()->segid);
        // }
        // output("triangles_og").set(triangles_og);
        // output("segment_ids_og").set(segment_ids_og);

        // Detect triangles with 3 short edges => collapse triangle to point
        // (remove 2 vertices)
        bool found_small_face;
        do {
          found_small_face = false;
          for (Finite_faces_iterator fit = tri.finite_faces_begin();
               fit != tri.finite_faces_end(); ++fit) {
            auto v0 = fit->vertex(0);
            auto v1 = fit->vertex(1);
            auto v2 = fit->vertex(2);
            auto& p0 = v0->point();
            auto& p1 = v1->point();
            auto& p2 = v2->point();
            // auto e0 = std::make_pair(fit, 0);
            // auto e1 = std::make_pair(fit, 1);
            // auto e2 = std::make_pair(fit, 2);
            // do not collapse if all vertices are on footprint boundary
            // if (
            //   v1->info() && v2->info() && v0->info()
            // )
            // continue;
            if ((CGAL::squared_distance(p0, p1) < sq_dist_thres &&
                 CGAL::squared_distance(p1, p2) < sq_dist_thres &&
                 CGAL::squared_distance(p2, p0) < sq_dist_thres)  //&& (
                //   tri.is_constrained( e0 ) &&
                //   tri.is_constrained( e1 ) &&
                //   tri.is_constrained( e2 )
                // )
            ) {
              // std::cout << "small triangle between " << p0 << " and " << p1
              // << "  and  " << p2 << std::endl;
              ConstraintSet constraints_to_restore;

              // collect incident constraint edges
              // std::vector<Edge> to_remove;
              for (size_t i = 0; i < 3; ++i) {
                auto vthis = fit->vertex(i);
                auto vexcept1 = fit->vertex(tri.cw(i));
                auto vexcept2 = fit->vertex(tri.ccw(i));
                get_incident_constraints(tri, vthis, vexcept1, vexcept2,
                                         constraints_to_restore);
              }

              // if (constraints_to_restore.size() > 50) continue;

              // collapse the triangle to centroid
              tri.remove_incident_constraints(v0);
              tri.remove(v0);
              tri.remove_incident_constraints(v1);
              tri.remove(v1);
              tri.remove_incident_constraints(v2);
              tri.remove(v2);

              // but first check points for being on the footprint boundary
              T::Point_2 pnew;
              if (v0->info()) {
                pnew = p0;
              } else if (v1->info()) {
                pnew = p1;
              } else if (v2->info()) {
                pnew = p2;
              } else {
                pnew = CGAL::centroid(p0, p1, p2);
              }
              restore_constraints(tri, pnew, constraints_to_restore);

              found_small_face = true;
              break;  // we need to restart the loop because we may have
                      // invalidated the finite faces iterator by modifying the
                      // faces of the triangulation
            }
          }

        } while (found_small_face);

        // Detect triangles with 1 short edge  => collapse the short edge to
        // point (remove one vertex)
        bool found_short_edge;
        do {
          found_short_edge = false;
          for (Finite_edges_iterator ceit = tri.finite_edges_begin();
               ceit != tri.finite_edges_end(); ++ceit) {
            if (tri.is_constrained(*ceit)) {
              auto v1 = ceit->first->vertex(tri.cw(ceit->second));
              auto v2 = ceit->first->vertex(tri.ccw(ceit->second));
              auto& p1 = v1->point();
              auto& p2 = v2->point();

              // do not collapse if this edge is on the footprint boundary
              // if (v1->info() && v2->info()) continue;

              if (CGAL::squared_distance(p1, p2) < sq_dist_thres) {
                // std::cout << "short edge between " << p1 << "  and  " << p2
                // << std::endl;

                // auto vi = ceit->second;
                ConstraintSet constraints_to_restore;
                get_incident_constraints(tri, v1, v1, v2,
                                         constraints_to_restore);
                get_incident_constraints(tri, v2, v1, v2,
                                         constraints_to_restore);

                // remove edge
                tri.remove_incident_constraints(v1);
                tri.remove(v1);
                tri.remove_incident_constraints(v2);
                tri.remove(v2);

                // insert midpoint and restore constraints
                // first check points for being on the footprint boundary
                T::Point_2 pnew;
                if (v1->info()) {
                  pnew = p1;
                } else if (v2->info()) {
                  pnew = p2;
                } else {
                  pnew = CGAL::midpoint(p1, p2);
                }
                restore_constraints(tri, pnew, constraints_to_restore);

                found_short_edge = true;
                break;
              }
            }
          }
        } while (found_short_edge);

        // Detect triangles with 1 vertex close to opposing (longest) edge  =>
        // remove long edge as constraint and ensure both short ones are
        // constrained
        // TODO: move the vertex opposed to long edge to be on the long edge ??
        // TODO: detect if an edge of the triangle is on the footprint boundary

        // bool found_small_face;
        // do {
        //   found_small_face = false;
        for (Finite_faces_iterator fit = tri.finite_faces_begin();
             fit != tri.finite_faces_end(); ++fit) {
          auto v0 = fit->vertex(0);
          auto v1 = fit->vertex(1);
          auto v2 = fit->vertex(2);
          auto& p0 = v0->point();
          auto& p1 = v1->point();
          auto& p2 = v2->point();
          auto e0 = std::make_pair(fit, 0);
          auto e1 = std::make_pair(fit, 1);
          auto e2 = std::make_pair(fit, 2);
          auto s0 = tri.segment(e0);
          auto s1 = tri.segment(e1);
          auto s2 = tri.segment(e2);
          if ((CGAL::squared_distance(s0, p0) < sq_dist_thres) &&
              tri.is_constrained(e0)) {
            if (tri.is_constrained(e1) || tri.is_constrained(e2)) {
              // std::cout << "flat triangle between " << s0 << " and " << p0 <<
              // std::endl;
              tri.remove_constrained_edge(fit, 0);
              if (!tri.is_constrained(e2)) tri.insert_constraint(v0, v1);
              if (!tri.is_constrained(e1)) tri.insert_constraint(v0, v2);
            }
          } else if ((CGAL::squared_distance(s1, p1) < sq_dist_thres) &&
                     tri.is_constrained(e1)) {
            if (tri.is_constrained(e0) || tri.is_constrained(e2)) {
              // std::cout << "flat triangle between " << s1 << " and " << p1 <<
              // std::endl;
              tri.remove_constrained_edge(fit, 1);
              if (!tri.is_constrained(e2)) tri.insert_constraint(v1, v0);
              if (!tri.is_constrained(e0)) tri.insert_constraint(v1, v2);
            }
          } else if ((CGAL::squared_distance(s2, p2) < sq_dist_thres) &&
                     tri.is_constrained(e2)) {
            if (tri.is_constrained(e0) || tri.is_constrained(e1)) {
              // std::cout << "flat triangle between " << s2 << " and " << p2 <<
              // std::endl;
              tri.remove_constrained_edge(fit, 2);
              if (!tri.is_constrained(e1)) tri.insert_constraint(v2, v0);
              if (!tri.is_constrained(e0)) tri.insert_constraint(v2, v1);
            }
          }
        }
        // } while (found_short_edge);

        std::vector<ForcedRegionLabel> forced_region_labels;
        label_final_regions(tri, walk_pl, arr, source_face_ids,
                            forced_region_labels);
        if (cfg.repair_non_manifold_vertices) {
          while (auto candidate = find_problematic_vertex(
                     tri, cfg.manifold_height_tolerance)) {
            forced_region_labels.push_back(repair_vertex(
                tri, *candidate, source_face_ids, cfg.manifold_repair_radius,
                cfg.manifold_height_tolerance));
            label_final_regions(tri, walk_pl, arr, source_face_ids,
                                forced_region_labels);
          }
        }

        // TriangleCollection triangles_snapped;
        // vec1i segment_ids_snapped;
        // for (auto fh = tri.finite_faces_begin(); fh !=
        // tri.finite_faces_end(); ++fh) {
        //   // only export triangles in the interior of a shape (thus excluding
        //   holes and exterior)

        //     arr3f p0 = {float (fh->vertex(0)->point().x()), float
        //     (fh->vertex(0)->point().y()), 0}; arr3f p1 = {float
        //     (fh->vertex(1)->point().x()), float (fh->vertex(1)->point().y()),
        //     0}; arr3f p2 = {float (fh->vertex(2)->point().x()), float
        //     (fh->vertex(2)->point().y()), 0}; triangles_snapped.push_back({
        //     p0,p1,p2 });
        //     // segment_ids_snapped.push_back(fh->info()->segid);
        //     // segment_ids_snapped.push_back(fh->info()->segid);
        //     // segment_ids_snapped.push_back(fh->info()->segid);
        // }
        // output("triangles_snapped").set(triangles_snapped);
        // output("segment_ids_snapped").set(segment_ids_snapped);

        // convert back from triangulation to arrangement
        Arrangement_2 arr_snap;
        std::unordered_map<T::Vertex_handle, Arrangement_2::Vertex_handle>
            vertex2arr_map;

        for (auto vh = tri.finite_vertices_begin();
             vh != tri.finite_vertices_end(); ++vh) {
          // make sure not to add isolated vertices
          if (tri.are_there_incident_constraints(vh)) {
            vertex2arr_map[vh] = insert_point(
                arr_snap,
                Arrangement_2::Point_2(vh->point().x(), vh->point().y()));
          }
        }

        for (auto ce : tri.constrained_edges()) {
          auto v1 = ce.first->vertex(tri.cw(ce.second));
          auto v2 = ce.first->vertex(tri.ccw(ce.second));
          auto& p1_ = v1->point();
          auto& p2_ = v2->point();
          auto p1 = Arrangement_2::Point_2(p1_.x(), p1_.y());
          auto p2 = Arrangement_2::Point_2(p2_.x(), p2_.y());

          // std::cout << p1 << "  --  " << p2 << std::endl;

          // if (vertex2arr_map[v1] != vertex2arr_map[v2]) {
          arr_snap.insert_at_vertices(Segment_2(p1, p2), vertex2arr_map[v1],
                                      vertex2arr_map[v2]);
          // } else {
          //   std::cout << "skipping edge between same vertex\n";
          // }
        }

        typedef CGAL::Arr_walk_along_line_point_location<Arrangement_2>
            Snap_walk_pl;
        Snap_walk_pl snap_walk_pl(arr_snap);
        std::unordered_map<Arrangement_2::Face_handle,
                           std::unordered_map<FaceInfo*, double>>
            output_face_labels;
        for (const auto& region : constrained_regions(tri)) {
          FaceInfo* region_label = nullptr;
          double region_area = 0;
          for (auto face : region) {
            if (region_label == nullptr) region_label = face->info();
            region_area += std::abs(tri.triangle(face).area());
          }
          if (region_label == nullptr || region_area == 0) continue;

          auto centroid = CGAL::centroid(tri.triangle(region.front()));
          auto object = snap_walk_pl.locate(
              Arrangement_2::Point_2(centroid.x(), centroid.y()));
          if (auto output_face = std::get_if<Face_const_handle>(&object)) {
            output_face_labels[arr_snap.non_const_handle(*output_face)]
                              [region_label] += region_area;
          }
        }

        arr_snap.unbounded_face()->data() = arr.unbounded_face()->data();
        for (auto output_face : arr_snap.face_handles()) {
          if (output_face->is_unbounded()) continue;
          const auto labels = output_face_labels.find(output_face);
          if (labels == output_face_labels.end() || labels->second.empty()) {
            throw roofer::rooferException(
                "Unable to transfer snapped arrangement face label");
          }
          auto best =
              std::max_element(labels->second.begin(), labels->second.end(),
                               [&](const auto& lhs, const auto& rhs) {
                                 if (lhs.second != rhs.second)
                                   return lhs.second < rhs.second;
                                 return source_face_ids.at(lhs.first) >
                                        source_face_ids.at(rhs.first);
                               });
          output_face->data() = *best->first;
        }

        // remove dangling edges if any, eg holes that collapse to a single edge
        // after snapping
        {
          std::vector<Arrangement_2::Halfedge_handle> to_remove;
          for (auto he : arr_snap.edge_handles()) {
            if (he->face() == he->twin()->face()) to_remove.push_back(he);
          }
          for (auto he : to_remove) {
            arr_snap.remove_edge(he);
          }
        }

        arr = arr_snap;
      }
    };

  }  // namespace arragementsnapper

  std::unique_ptr<ArrangementSnapperInterface> createArrangementSnapper() {
    return std::make_unique<arragementsnapper::ArrangementSnapper>();
  }

}  // namespace roofer::reconstruction
