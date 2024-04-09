#include "ArrangementBuilder.hpp"
#include "ArrangementBase.hpp"
#include "cgal_shared_definitions.hpp"
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>

namespace roofer::detection {

  class ArrangementBuilder : public ArrangementBuilderInterface{

    Arrangement_2::Vertex_handle arr_cross_edge(Arrangement_2& arr, Segment_2& segment, Arrangement_2::Halfedge_handle& edge, const double& dist_threshold) {
      auto& e_source = edge->source()->point();
      auto& e_target = edge->target()->point();
      #ifdef DEBUGGIE
        std::cout << "#arr_cross_edge\n";
      #endif
      // std::cout << "\te source " << e_source << std::endl;
      // std::cout << "\te target " << e_target << std::endl;
      // std::cout << "\ts degree=" << edge->source()->degree() << ", t degree=" << edge->target()->degree() << "\n" << std::endl;
      // Arrangement_2::Face_handle common_face;
      // arr_common_face(edge->source(), edge->target(), common_face);
      auto result = CGAL::intersection(Segment_2(e_source, e_target), segment);
      if (result) {
        if (auto p = boost::get<Point_2>(&*result)) {
          // std::cout << "\tp " << *p << std::endl;
          // std::cout << "\tp (geo) " << CGAL::to_double(p->x())+CGAL::to_double(geo_p.x()) << ", " << CGAL::to_double(p->y())+CGAL::to_double(geo_p.y()) << std::endl;
          // check if point of intersection is practically at the same coordinates of one of the edge's end points
          if (CGAL::squared_distance(*p, e_source) < dist_threshold) {
            return edge->source();
          } else if (CGAL::squared_distance(*p, e_target) < dist_threshold) {
            return edge->target();
          } else {
            // std::cout << "\tsplitting the edge at " << *p << std::endl;
            #ifdef DEBUGGIE
              std::cout << "POINT(" << CGAL::to_double(p->x()) << " " << CGAL::to_double(p->y()) << ")\n";
            #endif
            auto e = arr.split_edge(edge, Segment_2(e_source, *p), Segment_2(*p, e_target));
            return e->target(); // e->target is the split vertex
          }
        }
      } else {
        #ifdef DEBUGGIE
          std::cout << "#ERROR arr_cross_edge: no intersection!!!!!!" << std::endl;
        #endif
      }
      return Arrangement_2::Vertex_handle();
    }

    bool arr_edge_exists(Arrangement_2& arr, const Arrangement_2::Vertex_handle& s, const Arrangement_2::Vertex_handle& t) {
      auto segment = Segment_2(s->point(), t->point());
      auto he = s->incident_halfedges();
      auto first = he;
      do {
        auto v_next = he->source();
        if (v_next==t) {
          return true;
        }
      } while (++he!=first);
      return false;
    }

    void arr_insert(Arrangement_2& arr, Segment_2 segment, int& dist_threshold_exp) {
      std::list<CGAL::Object> zone_elems;
      
      Arrangement_2::Halfedge_handle edge;
      Arrangement_2::Vertex_handle vertex;
      Arrangement_2::Face_handle face;
      CGAL::zone(arr, segment, std::back_inserter(zone_elems));
      #ifdef DEBUGGIE
        std::cout << "#\n#Zone has " << zone_elems.size() << " elems\n";
      #endif
      // std::cout << "segment vector: " << segment.to_vector() << "\n";

      if (zone_elems.size()==0) {
        return;
      } else {

        // collect the vertices in the crossing of segment with the arrangement. Some are existing, some are created in this function.
        std::list<Arrangement_2::Vertex_handle> vertices;
        // std::pair<Arrangement_2::Vertex_handle, Arrangement_2::Vertex_handle> vertices;
        auto p_source = segment.min();
        auto p_target = segment.max();
        double dist_threshold = std::pow(10, -dist_threshold_exp);
        dist_threshold *= dist_threshold; // we compare only squared distances later

        // take care of a first face
        if(assign(face, zone_elems.front())) {
          if (zone_elems.size()==1) {
            #ifdef DEBUGGIE
              auto s_x = CGAL::to_double(p_source.x());
              auto s_y = CGAL::to_double(p_source.y());
              auto t_x = CGAL::to_double(p_target.x());
              auto t_y = CGAL::to_double(p_target.y());

              std::cout << "#insert_in_face_interior...\n";
              std::cout << "POINT(" << s_x << " " << s_y << ")\n";
              std::cout << "POINT(" << t_x << " " << t_y << ")\n";
              std::cout << "LINESTRING(" << s_x << " " << s_y << ", ";
              std::cout << t_x << " " << t_y << ")\n";
            #endif
            arr.insert_in_face_interior(segment, face);
            zone_elems.pop_front();
          } else {
            // remove first face and see what comes next
            zone_elems.pop_front();
            if(assign(edge, zone_elems.front())) {
              vertex = arr_cross_edge(arr, segment, edge, dist_threshold);
            } else {
              assign(vertex, zone_elems.front());
            }
            #ifdef DEBUGGIE
              std::cout << "insert_from_right_vertex..." << std::endl;
              std::cout << "POINT(" << CGAL::to_double(p_source.x()) << " " << CGAL::to_double(p_source.y()) << ")\n";
              std::cout << "LINESTRING(" << CGAL::to_double(p_source.x()) << " " << CGAL::to_double(p_source.y()) << ", ";
              std::cout << CGAL::to_double(vertex->point().x()) << " " << CGAL::to_double(vertex->point().y()) << ")\n";
            #endif
            arr.insert_from_right_vertex(Segment_2(p_source, vertex->point()), vertex);
            vertices.push_back(vertex);
          }
        }

        // collect and create further vertices
        while (zone_elems.size()) {

          // for a face we only are left with handling the case were face is the last element in the zone, otherwise skip faces
          if ( assign(face, zone_elems.front()) && (zone_elems.size()==1) ) {
            auto& v = vertices.front();
            
            #ifdef DEBUGGIE
              std::cout << "#inserting last edge in face vertices.size()==" << vertices.size() << "\n";
              std::cout << "#insert_from_left_vertex..." << std::endl;
              std::cout << "POINT(" << CGAL::to_double(p_target.x()) << " " << CGAL::to_double(p_target.y()) << ")\n";
              std::cout << "LINESTRING(" << CGAL::to_double(vertex->point().x()) << " " << CGAL::to_double(vertex->point().y()) << ", ";
              std::cout << CGAL::to_double(p_target.x()) << " " << CGAL::to_double(p_target.y()) << ")\n";
            #endif
            arr.insert_from_left_vertex(Segment_2(v->point(), p_target), v);
            vertices.pop_back(); //vertices should be empty after this

          // compute intersection and split edge
          } else if ( assign(edge, zone_elems.front()) ) {
            #ifdef DEBUGGIE
              std::cout << "#crossing edge...\n";
            #endif
            // std::cout << "edge source: " << edge->source()->point() << "\n";
            // std::cout << "edge target: " << edge->target()->point() << "\n";
            // std::cout << "edge vector: " << edge->to_vector() << "\n";
            vertices.push_back(arr_cross_edge(arr, segment, edge, dist_threshold));

          // vertex is easiest
          } else if ( assign(vertex, zone_elems.front()) ) {
            #ifdef DEBUGGIE
              std::cout << "#crossing vertex...\n";
            #endif
            vertices.push_back(vertex);
          } else {
            #ifdef DEBUGGIE 
              std::cout << "#ERROR unknown element in zone!!!\n";
            #endif
          }
          
          zone_elems.pop_front();
          
          #ifdef DEBUGGIE
            std::cout << "#checking..vertices.size()==" << vertices.size() << "\n";
          #endif
          if (vertices.size()==2) {
            auto& s = vertices.front();
            auto& t = vertices.back();
            // std::cout << "insert_at_vertices...";
            // std::cout << s->point() << std::endl;
            // std::cout << t->point() << std::endl;

            // find common face

            
            // correct for overlap
            Arrangement_2::Face_handle common_face;
            // if(arr_common_face(s, t, common_face)) {
              // s = arr_overlap_correction(arr, s, t, common_face, dist_threshold);
              // t = arr_overlap_correction(arr, t, s, common_face, dist_threshold);
              if(s==t) {
                #ifdef DEBUGGIE
                  std::cout << "#same vertex" << std::endl;
                #endif
              } else if (arr_edge_exists(arr,s,t)) {
                #ifdef DEBUGGIE
                  std::cout << "#edge already exists" << std::endl;
                #endif
              } else {
                #ifdef DEBUGGIE
                  std::cout << "#insert_at_vertices..." << std::endl;
                #endif
                // std::cout << "s degree=" << s->degree() << ", t degree=" << t->degree() << "\n" << std::endl;
                #ifdef DEBUGGIE
                  std::cout << "LINESTRING(" << CGAL::to_double(s->point().x()) << " " << CGAL::to_double(s->point().y()) << ", ";
                  std::cout << CGAL::to_double(t->point().x()) << " " << CGAL::to_double(t->point().y()) << ")\n";
                #endif

                arr.insert_at_vertices(Segment_2(s->point(), t->point()), s, t);
              }
            // }
            vertices.pop_front();
          }
        }
      }

      // size_t v_cnt=0, f_cnt=0, e_cnt=0;
      // for ( auto& object : zone_elems ) {
      //   if ( assign(face, object) ) {
      //     // std::cout << "Zone face, segid= " << face->data().segid << "\n";
      //     ++f_cnt;
      //   } else if ( assign(edge, object) ) {
      //     // std::cout << "Zone edge\n";
      //     ++e_cnt;
      //   } else if ( assign(vertex, object) ) {
      //     // std::cout << "Zone vertex\n";
      //     ++v_cnt;
      //   }
      // }
    }

    void arr_extend_insert_segment(Arrangement_2& arr, Segment_2 segment, const float& extension) {
      auto lv = segment.to_vector();
      lv = lv / CGAL::sqrt(CGAL::to_double(lv.squared_length()));
      #ifdef DEBUGGIE
        std::cout << "LINESTRING(" << CGAL::to_double((segment.source()-lv*extension).x()) << " " << CGAL::to_double((segment.source()-lv*extension).y()) << ", ";
        std::cout << CGAL::to_double((segment.target()+lv*extension).x()) << " " << CGAL::to_double((segment.target()+lv*extension).y()) << ")\n";
      #endif
      insert(arr, Segment_2(
        segment.source()-lv*extension,
        segment.target()+lv*extension
      ));
    }

    public:
    void compute(
      Arrangement_2& arrangement,
      LinearRing& footprint_,
      std::vector<EPECK::Segment_2>& input_edges,
      ArrangementBuilderConfig cfg
    ) override {

      typedef CGAL::Polygon_2<EPECK> Polygon_2;
      typedef CGAL::Polygon_with_holes_2<EPECK> Polygon_with_holes_2;
      typedef EPECK::Point_2 Point_2;
      
      // prepare footprint segments
      #ifdef DEBUGGIE
        std::cout << std::fixed << std::setprecision(4);
      #endif
      Polygon_with_holes_2 footprint;

      auto& lr = footprint_;
      Polygon_2 poly2;
      for (auto& p : lr) {
        poly2.push_back(Point_2(p[0], p[1]));
      }
      std::vector<Polygon_2> holes;
      for (auto& lr_hole : lr.interior_rings()) {
        Polygon_2 hole;
        for (auto& p : lr_hole) {
          hole.push_back(Point_2(p[0], p[1]));
        }
        holes.push_back(hole);
      }
      footprint = Polygon_with_holes_2(poly2, holes.begin(), holes.end());

      // insert footprint segments
      // Arrangement_2 arrangement;
      Face_split_observer obs (arrangement);
      {
        // insert(arr_base, footprint.outer_boundary().edges_begin(), footprint.outer_boundary().edges_end());
        // arr_insert_polygon(arr_base, footprint);
        // insert_non_intersecting_curves(arr_base, footprint.edges_begin(), footprint.edges_end());
        // if (!footprint.outer_boundary().is_simple()) {
        //   arr_filter_biggest_face(arr_base, rel_area_thres);
        // }
        for(auto e = footprint.outer_boundary().edges_begin(); e != footprint.outer_boundary().edges_end(); ++e) {
          arr_extend_insert_segment(arrangement, *e, cfg.fp_extension);
        }
        obs.set_hole_mode(true);
        for(auto hole = footprint.holes_begin(); hole != footprint.holes_end(); ++hole) {
          for(auto e = hole->edges_begin(); e != hole->edges_end(); ++e) {
            arr_extend_insert_segment(arrangement, *e, cfg.fp_extension);
          }
        }
        obs.set_hole_mode(false);
      }

      if(cfg.insert_lines) {
        
        typedef std::pair<Point_2, Point_2> PointPair;

        int arr_complexity = input_edges.size();
        // output("arr_complexity").set(arr_complexity);

        // if (lines_term.is_connected_type(typeid(linereg::Segment_2))) {
        for(size_t i=0; i<input_edges.size(); ++i) {
          auto& s = input_edges[i];
          if(cfg.insert_with_snap)
            arr_insert(arrangement, s, cfg.dist_threshold_exp);
          else {
            insert(arrangement, s);
          }
        }
      }
        // } else {
      //     std::vector<PointPair> segments;
      //     for(size_t i=0; i<lines_term.size(); ++i) {
      //       auto& s = lines_term.get<Segment>(i);
      //       Point_2 a(s[0][0],s[0][1]);
      //       Point_2 b(s[1][0],s[1][1]);
      //       segments.push_back(std::make_pair(a,b));
      //     }

      //     if (arr_complexity > max_arr_complexity) {
      //       std::sort(segments.begin(), segments.end(), [](PointPair& a, PointPair& b) {
      //         return CGAL::squared_distance(a.first,a.second) > CGAL::squared_distance(b.first,b.second);
      //       });
      //     }

      //     {
      //       std::vector<X_monotone_curve_2> lines;
      //       size_t i=0;
      //       for(auto& s : segments) {
      //         // Check for 0 segment length (quick fix for linux crash)
      //         if (i++ == max_arr_complexity) break;
      //         if (CGAL::squared_distance(s.first,s.second) > 0.001) lines.push_back(Segment_2(s.first,s.second));
      //       }
      //       insert(arrangement, lines.begin(), lines.end());
      //     }
      //   }
      // }
      
      //remove dangling edges
      {
        std::vector<Arrangement_2::Halfedge_handle> to_remove;
        for (auto he : arrangement.edge_handles()) {
          if (he->face()==he->twin()->face())
            to_remove.push_back(he);
        }
        for (auto he : to_remove) {
          arrangement.remove_edge(he);
        }
      }

      // if (snap_clean) arr_snapclean(arrangement, snap_dist, snap_detect_only);
    }
  };

  
  std::unique_ptr<ArrangementBuilderInterface> createArrangementBuilder() {
    return std::make_unique<ArrangementBuilder>();
  }

}