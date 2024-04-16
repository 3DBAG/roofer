// This file is part of gfp-building-reconstruction
// Copyright (C) 2018-2022 Ravi Peters

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include "ArrangementBase.hpp"
#include "ArrangementSnapper.hpp"
#include "cdt_util.hpp"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
// #include <CGAL/Constrained_triangulation_2.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>

#include <CGAL/Arr_walk_along_line_point_location.h>


namespace roofer::detection {

namespace arragementsnapper {

  typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
  typedef CGAL::Exact_predicates_inexact_constructions_kernel Epeck;
  typedef CGAL::Exact_predicates_tag Tag;

  typedef CGAL::Triangulation_vertex_base_2<K> VertexBase;
  typedef CGAL::Triangulation_vertex_base_with_info_2<bool, K, VertexBase> VertexBaseWithInfo;
  typedef CGAL::Constrained_triangulation_face_base_2<K> FaceBase;
  typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo*, K, FaceBase> FaceBaseWithInfo;
  typedef CGAL::Triangulation_data_structure_2<VertexBaseWithInfo, FaceBaseWithInfo> TriangulationDataStructure;
  typedef CGAL::Constrained_Delaunay_triangulation_2<K, TriangulationDataStructure, Tag> T;
  typedef T::Edge_circulator Edge_circulator;
  typedef T::Face_circulator Face_circulator;
  typedef T::Finite_faces_iterator Finite_faces_iterator;
  typedef T::Finite_edges_iterator Finite_edges_iterator;
  typedef T::Vertex_handle Vertex_handle;
  typedef T::Face_handle Face_handle;
  typedef std::pair<Face_handle, int> Edge;

  typedef std::unordered_map<Vertex_handle, FaceInfo*> ConstraintMap;

  // tri_util::CDT triangulate_polygon(LinearRing& poly, float dupe_threshold_exp=3) {
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

  void get_incident_constraints(T& tri, Vertex_handle& vthis, Vertex_handle& vexcept1, Vertex_handle vexcept2, ConstraintMap& constraints_to_restore) {
    // std::cout << "vthis degree=" << tri.degree(vthis) << std::endl;
    Edge_circulator ec = tri.incident_edges(vthis),
    done(ec);
    if (ec != nullptr) {
      do {
        if ( tri.is_constrained( *ec ) ) {
          // figure out which is the other vertex on this edge
          // and what is the face counter clockwise to the edge vthis -> vother
          auto va = ec->first->vertex(tri.cw(ec->second));
          auto vb = ec->first->vertex(tri.ccw(ec->second));
          Vertex_handle vother;
          Face_handle face_ccw;
          if (va == vthis) {
            vother = vb;
            face_ccw = ec->first->neighbor(ec->second);
          } else {
            vother = va;
            face_ccw = ec->first;
          };

          // check if face_ccw will be collapsed. If so we should not restore it.
          // vmirror is the vertex opposing ec in face_ccw
          // face_ccw will be collapsed if vmirror is one of the vexcept vertices
          // auto i = face_ccw->index(vthis);
          // auto vmirror =  face_ccw->vertex( tri.cw(i) );
          // if ( 
          //   vmirror == vexcept1 ||
          //   vmirror == vexcept2
          // ) {
          //   continue;
          // }

          // check if edge is not one that we will collapse
          if ( vother != vexcept1 && vother != vexcept2 ) { 
            constraints_to_restore[vother] = face_ccw->info();
          }
          // to_remove.push_back(*ec);
        }
      } while ( ++ec != done );
    }
  }

  void restore_constraints(T& tri, T::Point_2& pnew, ConstraintMap& constraints_to_restore) {
    auto vnew = tri.insert(pnew);

    // restore constraints
    // std::cout << "restoring " << constraints_to_restore.size() << " constraints\n";
    for (auto& [vh, finfo] : constraints_to_restore) {
      // std::cout << "reinsert constrained " << *vnew << " - " << *vh << std::endl;
      tri.insert_constraint(vnew, vh);

      // find the ccw face to the edge vnew->vh
      // Face_circulator fc = tri.incident_faces(vnew),
      //   done(fc);
      // if (fc != nullptr) {
      //   do { 
      //     if (fc->has_vertex(vh)) {
      //       // check if fc is ccw to edge vnew->vh
      //       if (vh == fc->vertex( tri.ccw( fc->index(vnew) ) ) ) {
      //         fc->info() = finfo;
      //       } else {
      //         // fc is cw to edge, so find the neighbor on the other side of edge
      //         fc->neighbor( tri.cw( fc->index(vh) ) )->info() = finfo;
      //       }
      //     }
      //   } while (++fc != done);
      // }
    }
  }

  class ArrangementSnapper : public ArrangementSnapperInterface {

    void compute(
        Arrangement_2& arr,
        ArrangementSnapperConfig cfg
      ) override {

      typedef CGAL::Arr_walk_along_line_point_location<Arrangement_2> Walk_pl;
      
      T tri;
      float sq_dist_thres = cfg.dist_thres*cfg.dist_thres;

      // map from arr vertices to tri vertices
      std::unordered_map<Arrangement_2::Vertex_handle, T::Vertex_handle> vertex_map;

      // Segment_list_2 seg_list;
      // Polyline_list_2 output_list;
      for (auto arrVertex : arr.vertex_handles()) {
        // auto& p = v->point();

        // check if this vertex is on the footprint
        Arrangement_2::Halfedge_around_vertex_circulator ec = arrVertex->incident_halfedges(),
        done(ec);
        bool has_ext_face(false), has_int_face(false);
        if (ec != nullptr) {
          do {
            has_ext_face = !ec->face()->data().in_footprint || has_ext_face;
            has_int_face = ec->face()->data().in_footprint || has_int_face;
          } while ( ++ec != done );
        }
        
        auto vtri = tri.insert(T::Point_2(
          CGAL::to_double(arrVertex->point().x()), 
          CGAL::to_double(arrVertex->point().y())
        ));
        vtri->info() = has_ext_face && has_int_face;
        vertex_map[arrVertex] = vtri;
      }


      Walk_pl walk_pl (arr);
      for (auto& arrEdge : arr.edge_handles()) {
        if ( vertex_map[ arrEdge->source() ] != vertex_map[ arrEdge->target() ]) {
          tri.insert_constraint(
            vertex_map[ arrEdge->source() ] ,
            vertex_map[ arrEdge->target() ]
          );
        }
      }

      // remove isolated vertices (sometimes these result from input arrangements with edge between 2 vertices with the same coordinates)
      // {
      //   std::vector<T::Vertex_handle> to_remove;
      //   for (auto vh = tri.finite_vertices_begin(); vh != tri.finite_vertices_end(); ++vh) {   
      //     if (!tri.are_there_incident_constraints(vh)) {
      //       to_remove.push_back(vh);
      //     }
      //   }
      //   for (auto& v: to_remove) {
      //     std:: cout << "removing vertex without incident constraints...\n"
      //     tri.remove(v);
      //   }
      // }

      // copy semantics
      for (auto fit : tri.finite_face_handles()) {

        auto p = CGAL::centroid(tri.triangle(fit));
        auto obj = walk_pl.locate( Walk_pl::Arrangement_2::Point_2(p.x(), p.y()) );

        // std::cout << "The point (" << p << ") is located ";
        if (auto f = boost::get<Face_const_handle>(&obj)) { // located inside a face
          // std::cout << "inside "
          //           << (((*f)->is_unbounded()) ? "the unbounded" : "a bounded")
          //           << " face." << std::endl;
          fit->info() = &(arr.non_const_handle(*f))->data(); 
        } else {
          // std::cout << "Failed point location in arrangement.\n";
        }
        // else if (auto e = boost::get<Halfedge_const_handle>(&obj)) // located on an edge
        //   std::cout << "on an edge: " << (*e)->curve() << std::endl;
        // else if (auto v = boost::get<Vertex_const_handle>(&obj)) // located on a vertex
        //   std::cout << "on " << (((*v)->is_isolated()) ? "an isolated" : "a")
        //             << " vertex: " << (*v)->point() << std::endl;
        // else CGAL_error_msg("Invalid object.");
      }

      // TriangleCollection triangles_og;
      // vec1i segment_ids_og;
      // for (auto fh = tri.finite_faces_begin(); fh != tri.finite_faces_end(); ++fh) {
      //   // only export triangles in the interior of a shape (thus excluding holes and exterior)

      //     arr3f p0 = {float (fh->vertex(0)->point().x()), float (fh->vertex(0)->point().y()), 0};
      //     arr3f p1 = {float (fh->vertex(1)->point().x()), float (fh->vertex(1)->point().y()), 0};
      //     arr3f p2 = {float (fh->vertex(2)->point().x()), float (fh->vertex(2)->point().y()), 0};
      //     triangles_og.push_back({ p0,p1,p2 });
      //     segment_ids_og.push_back(fh->info()->segid);
      //     segment_ids_og.push_back(fh->info()->segid);
      //     segment_ids_og.push_back(fh->info()->segid);
      // }
      // output("triangles_og").set(triangles_og);
      // output("segment_ids_og").set(segment_ids_og);

      // Detect triangles with 3 short edges => collapse triangle to point (remove 2 vertices)
      bool found_small_face;
      do {
        found_small_face = false;
        for (Finite_faces_iterator fit = tri.finite_faces_begin(); fit != tri.finite_faces_end(); ++fit) {
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
          if (
            (
              CGAL::squared_distance(p0, p1) < sq_dist_thres &&
              CGAL::squared_distance(p1, p2) < sq_dist_thres &&
              CGAL::squared_distance(p2, p0) < sq_dist_thres
            ) //&& (
            //   tri.is_constrained( e0 ) &&
            //   tri.is_constrained( e1 ) &&
            //   tri.is_constrained( e2 )
            // )
          ) {
            // std::cout << "small triangle between " << p0 << " and " << p1 << "  and  " << p2 << std::endl;
            ConstraintMap constraints_to_restore;

            // collect incident constraint edges
            // std::vector<Edge> to_remove;
            for (size_t i=0; i<3; ++i) {
              auto vthis = fit->vertex(i);
              auto vexcept1 = fit->vertex(tri.cw(i));
              auto vexcept2 = fit->vertex(tri.ccw(i));
              get_incident_constraints(tri, vthis, vexcept1, vexcept2, constraints_to_restore);
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
              pnew = CGAL::centroid(p0,p1,p2);
            }
            restore_constraints(tri, pnew, constraints_to_restore);
            
            found_small_face = true;
            break; // we need to restart the loop because we may have invalidated the finite faces iterator by modifying the faces of the triangulation 
          }
        }

      } while (found_small_face);

      // Detect triangles with 1 short edge  => collapse the short edge to point (remove one vertex)
      bool found_short_edge;
      do {
        found_short_edge = false;
        for (Finite_edges_iterator ceit = tri.finite_edges_begin(); ceit != tri.finite_edges_end(); ++ceit) {
          if (tri.is_constrained(*ceit)) {
            auto v1 = ceit->first->vertex(tri.cw(ceit->second));
            auto v2 = ceit->first->vertex(tri.ccw(ceit->second));
            auto& p1 = v1->point();
            auto& p2 = v2->point();
            
            // do not collapse if this edge is on the footprint boundary
            // if (v1->info() && v2->info()) continue;

            if (CGAL::squared_distance(p1, p2) < sq_dist_thres) {
              // std::cout << "short edge between " << p1 << "  and  " << p2 << std::endl;

              // auto vi = ceit->second;
              ConstraintMap constraints_to_restore;
              get_incident_constraints(tri, v1, v1, v2, constraints_to_restore);
              get_incident_constraints(tri, v2, v1, v2, constraints_to_restore);

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

      // Detect triangles with 1 vertex close to opposing (longest) edge  => remove long edge as constraint and ensure both short ones are constrained
      // TODO: move the vertex opposed to long edge to be on the long edge ??
      // TODO: detect if an edge of the triangle is on the footprint boundary

      // bool found_small_face;
      // do {
      //   found_small_face = false;
        for (Finite_faces_iterator fit = tri.finite_faces_begin(); fit != tri.finite_faces_end(); ++fit) {
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
          if ( (CGAL::squared_distance(s0, p0) < sq_dist_thres ) && tri.is_constrained( e0 ) ) {
            if ( tri.is_constrained( e1 ) || tri.is_constrained( e2 ) ) {
              // std::cout << "flat triangle between " << s0 << " and " << p0 << std::endl;
              tri.remove_constrained_edge(fit, 0);
              if ( !tri.is_constrained( e2 ) ) tri.insert_constraint(v0, v1);
              if ( !tri.is_constrained( e1 ) ) tri.insert_constraint(v0, v2);
            }
          } else if ( (CGAL::squared_distance(s1, p1) < sq_dist_thres) && tri.is_constrained( e1 ) ) {
            if ( tri.is_constrained( e0 ) || tri.is_constrained( e2 ) ) {
              // std::cout << "flat triangle between " << s1 << " and " << p1 << std::endl;
              tri.remove_constrained_edge(fit, 1);
              if ( !tri.is_constrained( e2 ) ) tri.insert_constraint(v1, v0);
              if ( !tri.is_constrained( e0 ) ) tri.insert_constraint(v1, v2);
            }
          } else if ( (CGAL::squared_distance(s2, p2) < sq_dist_thres) && tri.is_constrained( e2 ) ) {
            if ( tri.is_constrained( e0 ) || tri.is_constrained( e1 ) ) {
              // std::cout << "flat triangle between " << s2 << " and " << p2 << std::endl;
              tri.remove_constrained_edge(fit, 2);
              if ( !tri.is_constrained( e1 ) ) tri.insert_constraint(v2, v0);
              if ( !tri.is_constrained( e0 ) ) tri.insert_constraint(v2, v1);
            }
          }
        }
      // } while (found_short_edge);

      // TriangleCollection triangles_snapped;
      // vec1i segment_ids_snapped;
      // for (auto fh = tri.finite_faces_begin(); fh != tri.finite_faces_end(); ++fh) {
      //   // only export triangles in the interior of a shape (thus excluding holes and exterior)

      //     arr3f p0 = {float (fh->vertex(0)->point().x()), float (fh->vertex(0)->point().y()), 0};
      //     arr3f p1 = {float (fh->vertex(1)->point().x()), float (fh->vertex(1)->point().y()), 0};
      //     arr3f p2 = {float (fh->vertex(2)->point().x()), float (fh->vertex(2)->point().y()), 0};
      //     triangles_snapped.push_back({ p0,p1,p2 });
      //     // segment_ids_snapped.push_back(fh->info()->segid);
      //     // segment_ids_snapped.push_back(fh->info()->segid);
      //     // segment_ids_snapped.push_back(fh->info()->segid);
      // }
      // output("triangles_snapped").set(triangles_snapped);
      // output("segment_ids_snapped").set(segment_ids_snapped);

      // convert back from triangulation to arrangement
      Arrangement_2 arr_snap;
      std::unordered_map<T::Vertex_handle, Arrangement_2::Vertex_handle> vertex2arr_map;

      for (auto vh = tri.finite_vertices_begin(); vh != tri.finite_vertices_end(); ++vh) {
          // make sure not to add isolated vertices
          if (tri.are_there_incident_constraints(vh)) {
            vertex2arr_map[vh] = insert_point( arr_snap, Arrangement_2::Point_2(vh->point().x(), vh->point().y()) );
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
          arr_snap.insert_at_vertices(
            Segment_2( p1, p2 ),
            vertex2arr_map[v1],
            vertex2arr_map[v2]
          );
        // } else {
        //   std::cout << "skipping edge between same vertex\n";
        // }
      }

      for (auto& arrFace : arr_snap.face_handles()) {
        LinearRing poly;
        if(arrFace->is_fictitious()) continue;
        
        if (arrangementface_to_polygon(arrFace, poly)) {

          // std::cout << "poly size: " << poly.size() << std::endl;
          tri_util::CDT cdt = tri_util::create_from_polygon(poly);

          std::unordered_map<Arrangement_2::Face_handle, float> canidate_faces;
          for (tri_util::CDT::Finite_faces_iterator fit = cdt.finite_faces_begin();
            fit != cdt.finite_faces_end(); ++fit) {

            if (!fit->info().in_domain()) continue;
            auto p = CGAL::centroid(cdt.triangle(fit));

            // !! this turns out to be messy
            // auto snap_triangle = tri.locate(p);
            // arrFace->data() = *snap_triangle->info();

            auto obj = walk_pl.locate( Walk_pl::Arrangement_2::Point_2(p.x(), p.y()) );

            if (auto f = boost::get<Face_const_handle>(&obj)) { // located inside a face
              // arrFace->data() = (*f)->data();
              canidate_faces[arr.non_const_handle(*f)] += cdt.triangle(fit).area();
            }
            // break;
          }

          // pick the candidate with the largest overlapping area
          // std::cerr << "Size=" << canidate_faces.size() << std::endl;
          if (canidate_faces.size()) {
            auto best_face = std::max_element(canidate_faces.begin(), canidate_faces.end(),
              [](const std::pair<Arrangement_2::Face_handle, float>& p1, const std::pair<Arrangement_2::Face_handle, float>& p2) {
                  return p1.second < p2.second;
              }
            );
            arrFace->data() = best_face->first->data();
          } else {
            std::cout << "Unable to locate overlapping triangle\n";
            arrFace->data().is_finite=true;
            arrFace->data().is_ground=true;
            arrFace->data().in_footprint=false;
            arrFace->data().is_footprint_hole=false;
          }

        }
      }

      // remove dangling edges if any, eg holes that collapse to a single edge after snapping
      {
        std::vector<Arrangement_2::Halfedge_handle> to_remove;
        for (auto he : arr_snap.edge_handles()) {
          if (he->face()==he->twin()->face())
            to_remove.push_back(he);
        }
        for (auto he : to_remove) {
          arr_snap.remove_edge(he);
        }
      }

      arr = arr_snap;
    }
  };

} // namespace arragementsnapper

  std::unique_ptr<ArrangementSnapperInterface> createArrangementSnapper() {
    return std::make_unique<arragementsnapper::ArrangementSnapper>();
  }

} // namespace roofer::detection