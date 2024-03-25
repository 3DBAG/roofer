#include "AlphaShaper.hpp"

// 2D alpha shapes
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Alpha_shape_2.h>
#include <CGAL/Alpha_shape_vertex_base_2.h>
#include <CGAL/Alpha_shape_face_base_2.h>
#include <CGAL/Projection_traits_xy_3.h>

namespace roofer::detection {
  static const int EXTERIOR=-1, NEVER_VISITED=-2, HOLE=-3;

  typedef CGAL::Exact_predicates_inexact_constructions_kernel  K;
  typedef CGAL::Projection_traits_xy_3<K>								       Gt;
  typedef K::FT                                                FT;
  
  typedef CGAL::Alpha_shape_vertex_base_2<Gt>                  Vb;
  struct FaceInfo {
    int label = NEVER_VISITED;
    bool incident_ring_is_extracted = false;
  };
  typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo,K>    Fbb;
  typedef CGAL::Alpha_shape_face_base_2<Gt,Fbb>                    Fb;

  typedef CGAL::Triangulation_data_structure_2<Vb,Fb>          Tds;
  typedef CGAL::Delaunay_triangulation_2<Gt,Tds>               Triangulation_2;
  typedef CGAL::Alpha_shape_2<Triangulation_2>                 Alpha_shape_2;
  typedef Alpha_shape_2::Vertex_handle                        Vertex_handle;
  typedef Alpha_shape_2::Edge                                 Edge;
  typedef Alpha_shape_2::Face_handle                          Face_handle;
  typedef Alpha_shape_2::Vertex_handle                        Vertex_handle;
  typedef Alpha_shape_2::Vertex_circulator                    Vertex_circulator;
  typedef Alpha_shape_2::Edge_circulator                      Edge_circulator;

  class AlphaShapeRegionGrower {
    Alpha_shape_2 &A;
    enum Mode {
      LABEL_INFINITE_FACE, // stop at alpha boundary
      LABEL_INTERIOR_FACE, // stop at faces labels as exterior
      LABEL_HOLE_FACE
    };
    int label_cnt = 0; // label==-1 means exterior, -2 mean never visited, -3 means a hole, 0+ means a regular region
    int hole_cnt = HOLE;

    public:
    std::unordered_map<int, LinearRing> region_map; //label: (boundary vertex)
    AlphaShapeRegionGrower(Alpha_shape_2& as) : A(as) {};
    
    template<typename RingType> 
    void extract_ring(Vertex_handle v_start, int label_region, int label_other, RingType& ring) {
      // TODO: fix infinite loop that may occur when alpha is not computed with optimal alpha (ie it is to small, probably causing many singular edges etc)
      // find edges of outer boundary in order keeping the interior of the polygon in the left side 
      // (thus CCW for exterior ring, CW for holes)
      // als label the adjacent interior faces (in case they had not been visited yet) while we are walking around them anyways..
      // (also ensure we are only extracting this ring once)
      // NB: assumes we have no singular edges in the a-shape!

      // std::cout << "extracting ring withe label_other=" << label_other << std::endl;

      ring.push_back( {float(v_start->point().x()), float(v_start->point().y()), float(v_start->point().z())} );
      // secondly, walk along the entire boundary starting from v_start
      Vertex_handle v_next, v_prev = v_start, v_cur = v_start;
      // size_t extract_cnt=0;
      bool firstv=true;
      Face_handle f_prev;
      do {
        Edge_circulator ec, done;
        if(firstv) {
          ec = A.incident_edges(v_cur);
          firstv=false;
        } else {
          ec = A.incident_edges(v_cur, f_prev);
        }
        done = ec;
        do {
          // very rare case (kadaster tile 6711 fid 411) ec does not point to a face. Not sure how this is possible.
          if(ec->first==nullptr) break;

          // if(A.classify(*ec)==Alpha_shape_2::SINGULAR)
          // std::cout << "consider ec, singular?=" << (A.classify(*ec)==Alpha_shape_2::SINGULAR? "yes":"no") << std::endl;
          // find the vertex on the other side of the incident edge ec
          auto v = ec->first->vertex(A.cw(ec->second));
          if (v_cur == v) v = ec->first->vertex(A.ccw(ec->second));
          // find labels of two adjacent faces
          auto face1 = ec->first;
          auto vertex1 = face1->vertex(ec->second);
          auto face2 = ec->first->neighbor(ec->second);
          auto vertex2 = A.mirror_vertex(face1, ec->second);
          auto label1 = face1->info().label;
          auto label2 = face2->info().label;
          bool extract_edge = false;
          // check if the edge is on the boundary of the region and if we are keeping the label_region on the left
          if (label1==label_region || label1==NEVER_VISITED) {
            if (label2==label_other) {
              auto p_cur = v_cur->point();
              auto p = v->point();
              auto p1 = vertex1->point();
              if (CGAL::LEFT_TURN == CGAL::orientation(
                K::Point_2(p_cur.x(), p_cur.y()), 
                K::Point_2(p.x(), p.y()), 
                K::Point_2(p1.x(), p1.y())
                )) {
                extract_edge = true;
                face2->info().incident_ring_is_extracted = true;
                f_prev = face1;
              }
            }
          } else if (label2==label_region || label2==NEVER_VISITED) {
            if (label1==label_other) {
              auto p_cur = v_cur->point();
              auto p = v->point();
              auto p2 = vertex2->point();
              if (CGAL::LEFT_TURN == CGAL::orientation(
                K::Point_2(p_cur.x(), p_cur.y()), 
                K::Point_2(p.x(), p.y()), 
                K::Point_2(p2.x(), p2.y())
                )) {
                extract_edge = true;
                face1->info().incident_ring_is_extracted = true;
                f_prev = face2;
              }
            }
          }
          if(extract_edge  && (v != v_prev)) {
            v_next = v;
            ring.push_back( {float(v_next->point().x()), float(v_next->point().y()), float(v_next->point().z())} );
            if(ring.size()>1){
              auto last = ring.rbegin();
              auto p = *last;
              ++last;
              auto p_prev = *last;
              // std::cout << "POINT(" << p[0] << " " <<  p[1] << "); " << ++extract_cnt << "\n";
              // std::cout << "LINESTRING(" << p_prev[0] << " " << p_prev[1] << ", ";
              // std::cout << p[0] << " " << p[1] << ")\n";
              // if(++extract_cnt>2000) exit(1);
            }
            // std::cout << "ring size = " << ring.size() << std::endl;
            break;
          }
        } while (++ec != done);

        // break in case we are not finding next edges to walk to
        if(v_cur == v_next) break;

        v_prev = v_cur;
        v_cur = v_next;
        

      } while (v_next != v_start);
    }

    void grow(bool extract_polygons) {
      std::stack<Face_handle> interior_seeds, hole_seeds;
      // label triangles reachable from inf face as -1; ie exterior
      auto inf_face = A.infinite_face();
      inf_face->info().label = EXTERIOR;
      grow_region(inf_face, LABEL_INFINITE_FACE); // this sets label of exterior faces to -1
      // label remaining faces that are not interior in the a-shape as -3, and push faces that are inside the a-shape on the seeds stack
      for (auto fh = A.all_faces_begin(); fh!=A.all_faces_end(); ++fh) {
        if (fh->info().label != EXTERIOR) {
          if (A.classify(fh) == Alpha_shape_2::INTERIOR) {
            interior_seeds.push(fh);
          } else {
            hole_seeds.push(fh);
            // fh->info().label = HOLE;
          }
        }
      }
      ;
      while (!hole_seeds.empty()) {
        auto fh = hole_seeds.top(); hole_seeds.pop();
        if (fh->info().label == NEVER_VISITED) {
          fh->info().label = hole_cnt;
          grow_region(fh, LABEL_HOLE_FACE);
          --hole_cnt;
        }
      }
      while (!interior_seeds.empty()) {
        auto fh = interior_seeds.top(); interior_seeds.pop();
        if (fh->info().label == NEVER_VISITED) {
          fh->info().label = label_cnt;
          grow_region(fh, LABEL_INTERIOR_FACE, extract_polygons);
          ++label_cnt;
        }
      }
    }

    void grow_region (Face_handle face_handle, Mode mode, bool extract_polygons=false) {
      std::stack<Face_handle> candidates;
      candidates.push(face_handle);

      while(candidates.size()>0) {
        auto fh = candidates.top(); candidates.pop();
        // check the 3 neighbors of this face
        for (int i=0; i<3; ++i) {
          auto e = std::make_pair(fh,i);
          auto neighbor = fh->neighbor(i);
          
          // check if neighbor handle is not pointing anywhere. Should not happen, but it does occasionally in practice.
          if (neighbor == nullptr) {
            // std::cout << "null\n" << std::flush; 
            continue;
          }

          if (mode == LABEL_INFINITE_FACE) {
            // add neighbor if it is not on the ohter side of alpha boundary
            // check if this neighbor hasn't been visited before
            if (neighbor->info().label == NEVER_VISITED) {
              auto edge_class = A.classify(e);
              if ( ! (edge_class == Alpha_shape_2::REGULAR) ) {
                neighbor->info().label = EXTERIOR;
                candidates.push(neighbor);
              }
            }
          } else if (mode == LABEL_HOLE_FACE){
            if (neighbor->info().label == NEVER_VISITED) {
              auto edge_class = A.classify(e);
              if ( ! (edge_class == Alpha_shape_2::REGULAR) ) {
                neighbor->info().label = hole_cnt;
                candidates.push(neighbor);
              }
            }
          } else if (mode == LABEL_INTERIOR_FACE) {
            // check if this neighbor hasn't been visited before and is not exterior/hole
            if (neighbor->info().label == NEVER_VISITED) {
              neighbor->info().label = label_cnt;
              candidates.push(neighbor);
            // if it is exterior/hole, we find extract a ring
            } else if (extract_polygons && (neighbor->info().label == EXTERIOR || neighbor->info().label <= HOLE)) {
              if( region_map.find(label_cnt)==region_map.end() ) {
                region_map[label_cnt] = LinearRing();
              }
              auto ring_vertex = fh->vertex(A.cw(i));
              if (neighbor->info().label == EXTERIOR && !neighbor->info().incident_ring_is_extracted) { // if it is exterior, we find extract the exterior ring
                extract_ring(ring_vertex, label_cnt, EXTERIOR, region_map[label_cnt]);
              } else if (neighbor->info().label <= HOLE && !neighbor->info().incident_ring_is_extracted) { // if it is a hole, we extract the interior ring
                vec3f interior_ring;
                extract_ring(ring_vertex, label_cnt, neighbor->info().label, interior_ring);
                region_map[label_cnt].interior_rings().push_back(interior_ring);
              }
            }
          }

        }
      }
    }
  };

class AlphaShaper : public AlphaShaperInterface {
  
  void compute(const IndexedPlanesWithPoints& pts_per_roofplane, AlphaShaperConfig cfg) override {
    std::cout << std::fixed << std::setprecision(4);

    PointCollection edge_points;
    LineStringCollection alpha_edges;
    
    std::vector<Triangulation_2> alpha_dts;
    vec1i segment_ids;
    for (auto& it : pts_per_roofplane ) {
      if (it.first == -1) continue; // skip points if they put at index -1 (eg if we care not about slanted surfaces for ring extraction)
      auto points = it.second.second;
      if(points.size() < 3) continue;
      Triangulation_2 T;
      T.insert(points.begin(), points.end());
      Alpha_shape_2 A(T,
                  FT(cfg.thres_alpha),
                  Alpha_shape_2::GENERAL);
      
      double alpha = cfg.thres_alpha;
      if (cfg.optimal_alpha && cfg.optimal_only_if_needed) {
        alpha = std::max(float(*A.find_optimal_alpha(1)), cfg.thres_alpha);
      } else if (cfg.optimal_alpha) {
        alpha = *A.find_optimal_alpha(1);
      }
      A.set_alpha(FT(alpha));

      for (auto it = A.alpha_shape_vertices_begin(); it!=A.alpha_shape_vertices_end(); it++) {
        auto p = (*it)->point();
        edge_points.push_back({float(p.x()), float(p.y()), float(p.z())});
      }
      for (auto it = A.alpha_shape_edges_begin(); it!=A.alpha_shape_edges_end(); it++) {
        auto p1 = it->first->vertex(A.cw(it->second))->point();
        auto p2 = it->first->vertex(A.ccw(it->second))->point();

        alpha_edges.push_back({
          {float(p1.x()), float(p1.y()), float(p1.z())},
          {float(p2.x()), float(p2.y()), float(p2.z())}
        });
      }
      
      // flood filling 
      auto grower = AlphaShapeRegionGrower(A);
      grower.grow(cfg.extract_polygons);

      // collect triangles
      alpha_triangles;
      for (auto fh = A.finite_faces_begin(); fh != A.finite_faces_end(); ++fh) {
        // only export triangles in the interior of a shape (thus excluding holes and exterior)
        if(fh->info().label>=0) {
          arr3f p0 = {float (fh->vertex(0)->point().x()), float (fh->vertex(0)->point().y()), float (fh->vertex(0)->point().z())};
          arr3f p1 = {float (fh->vertex(1)->point().x()), float (fh->vertex(1)->point().y()), float (fh->vertex(1)->point().z())};
          arr3f p2 = {float (fh->vertex(2)->point().x()), float (fh->vertex(2)->point().y()), float (fh->vertex(2)->point().z())};
          alpha_triangles.push_back({ p0,p1,p2 });
          segment_ids.push_back(fh->info().label);
          segment_ids.push_back(fh->info().label);
          segment_ids.push_back(fh->info().label);
        }
      }

      for (auto& kv : grower.region_map) {
        // finally, store the ring 
        if(kv.second.size() > 2) {
          alpha_rings.push_back(kv.second);
          roofplane_ids.push_back(it.first);
        }
        // alpha_dts.push_back(T);
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

}