#include "ArrangementBase.hpp"
#include "ArrangementExtruder.hpp"

namespace roofer::detection {

  // typedef CGAL::Cartesian<float>           AK;

  inline arr3f v2p(Arrangement_2::Vertex_handle v, float h) {
    return {
            float(CGAL::to_double(v->point().x())),
            float(CGAL::to_double(v->point().y())),
            h
          };
  }
  template<typename P> arr3f p2p(P p) {
    return {
            float(CGAL::to_double(p->x())),
            float(CGAL::to_double(p->y())),
            float(CGAL::to_double(p->z()))
          };
  }

  typedef std::pair<float, Face_handle> hf_pair;
  vec3f get_heights(std::vector<hf_pair>& vertex_column, Vertex_handle v, Face_handle f_a, Face_handle f_b, float& h_a, float& h_b) {
    vec3f v_other;
    float h_prev = -999999;
    bool found_first = false;
    for(auto& [h,face] : vertex_column) {
      if (face==f_a) {
        h_a = h;
        if (!found_first) 
          found_first = true;
        else {
          if (h==h_prev && v_other.size()) {
            v_other.erase(v_other.end()-1);
          }
          break;
        }
      } else if (face==f_b) {
        h_b = h;
        if (!found_first) 
          found_first = true;
        else {
          if (h==h_prev && v_other.size()) {
            v_other.erase(v_other.end()-1);
          }
          break;
        }
      } else if (found_first) {
        if (h!=h_prev)
          v_other.push_back(v2p(v, h));
      } //else break;
      h_prev = h;
    }
    return v_other;
  }

  template<typename T> void push_ccb(
    T& ring, 
    Halfedge_handle hedge, 
    std::unordered_map<Vertex_handle, std::vector<hf_pair>>& vertex_columns, 
    std::unordered_map<Halfedge_handle, 
    EPECK::Point_3>& extra_wall_points, 
    float& snap_tolerance) {

    auto first = hedge;
    do {
      auto v = hedge->source();
      if(CGAL::squared_distance(v->point(), hedge->target()->point()) > snap_tolerance) {
        for(auto& [h,f_h] : vertex_columns[v]) {
          if (f_h==hedge->face()) {
            ring.push_back(v2p(v,h));
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
    } while (hedge!=first);
  }
  
  class ArrangementExtruder : public ArrangementExtruderInterface{

    public:
    void compute(
        Arrangement_2& arr,
        const ElevationProvider& elevation_provider,
        ArrangementExtruderConfig cfg
    ) override {
      typedef Arrangement_2::Traits_2 AT;
      float snap_tolerance = std::pow(10,-cfg.snap_tolerance_exp);

      // assume we have only one unbounded faces that just has the building footprint as a hole

      auto unbounded_face = arr.unbounded_face();
      unbounded_face->data().elevation_70p=elevation_provider.get_percentile(0.7);
      unbounded_face->data().elevation_97p=elevation_provider.get_percentile(0.97);

      // floor
      if (cfg.do_floor) {
        // std::cout << "arrangement has " << arr.number_of_unbounded_faces() << "unbounded faces\n";
        // there should only be one hole in the unbounded face (building consists of one part)
        for(Arrangement_2::Hole_iterator floorpart_ = unbounded_face->holes_begin(); floorpart_ != unbounded_face->holes_end(); ++floorpart_ ) {
          LinearRing floor;
          auto he = *floorpart_;
          auto first = he;
          do {
            if(CGAL::squared_distance(he->source()->point(), he->target()->point()) > snap_tolerance) {
              float pt_elevation = elevation_provider.get(he->source()->point());
              floor.push_back(v2p(he->source(), pt_elevation));
            }
            he = he->next();
          } while(he!=first);

          faces.push_back(floor);
          labels.push_back(int(0));

          // create a mesh
          Mesh mesh;
          mesh.push_polygon(floor, int(0));
          multisolid[he->twin()->face()->data().part_id] = mesh;
          // mesh.push_attribute("surface_type", int(0));
        }
        // get the holes
        for (auto face: arr.face_handles()) {
          if (face->data().is_footprint_hole) {
            vec3f hole;
            auto he = face->outer_ccb();
            auto first = he;
            do {
              if(CGAL::squared_distance(he->source()->point(), he->target()->point()) > snap_tolerance) {
                float pt_elevation = elevation_provider.get(he->source()->point());
                hole.push_back(v2p(he->source(), pt_elevation));
              }
              he = he->next();
            } while(he!=first);

            // attach hole to appropriate mesh and linear ring
            const auto part_id = he->twin()->face()->data().part_id;
            if(hole.size()!=0 && multisolid.find(part_id)!=multisolid.end()) {
              auto& floor = multisolid[part_id].get_polygons()[0];
              floor.interior_rings().push_back(hole);
            } else { std::cout << "skipping a hole for which no polygon exists...\n"; }
          }
        }
      }



      // check for no detected planes (if one face inside fp has none then all have none)
      for (const auto f : arr.face_handles()) {
        if (f->data().in_footprint && f->data().segid==0) {
          meshes.push_back(multisolid[0]);
          return;
        }
      }


      // compute all heights for each vertex
      std::unordered_map<Vertex_handle, std::vector<hf_pair>> vertex_columns;
      for(auto& v : arr.vertex_handles()) {
        auto& p = v->point();
        auto he = v->incident_halfedges();
        auto first = he;
        std::vector<hf_pair> heights;
        do {
          auto f = he->face();
          float h;
          if (f->data().in_footprint && f->data().segid!=0) {
            if(cfg.LoD2) {
              auto& plane = f->data().plane;
              h = (plane.a()*CGAL::to_double(p.x()) + plane.b()*CGAL::to_double(p.y()) + plane.d()) / (-plane.c());
            } else {
              if (cfg.lod1_extrude_to_max_)
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
        } while (++he!=first);
        
        if(heights.size()==0) continue;
        
        // sort heights
        std::sort(heights.begin(), heights.end(), [](hf_pair& a, hf_pair& b) {
          return a.first < b.first;   
        });
        // equalise heights that are practically the same
        float h_ref = heights[0].first;
        for (auto& [h,face] : heights) {
          if(std::fabs(h_ref-h)<snap_tolerance) {
            h = h_ref;
          }
          h_ref = h;
        }

        vertex_columns[v]=heights;
      }

      // walls
      // store points that need to be created to do the walls right. We need them later for the roofs
      std::unordered_map<Halfedge_handle, EPECK::Point_3> extra_wall_points;
      if (cfg.do_walls) {
        for (auto edge : arr.edge_handles()) {
          auto e_a = edge->twin();
          auto e_b = edge;
          auto v1 = e_a->target();
          auto v2 = e_a->source();
          auto& p1 = v1->point();
          auto& p2 = v2->point();
          auto f_a = e_a->face();
          auto f_b = e_b->face();
          bool fp_a = f_a->data().in_footprint;
          bool fp_b = f_b->data().in_footprint;
          auto& pl_a = f_a->data().plane;
          auto& pl_b = f_b->data().plane;

          // edge has no length
          if(CGAL::squared_distance(p1,p2)<snap_tolerance)
            continue;

          // edge is not in footprint nor on its boundary
          if (!fp_a & !fp_b) {
            continue;
          }

          // get precomputed heights from vertex column
          float h1a, h1b, h2a, h2b;
          vec3f v1_other = get_heights(vertex_columns[v1], v1, f_a, f_b, h1a, h1b);
          vec3f v2_other = get_heights(vertex_columns[v2], v2, f_a, f_b, h2a, h2b);
          
          // // set base (ground) elevation to vertices adjacent to a face oustide the building fp
          int part_id;
          if (fp_a && !fp_b) {
            h1b = elevation_provider.get(p1);
            h2b = elevation_provider.get(p2);
            part_id = f_a->data().part_id;
          } else if (!fp_a && fp_b) {
            h1a = elevation_provider.get(p1);
            h2a = elevation_provider.get(p2);
            part_id = f_b->data().part_id;
          } else{ // both sides have the same part_id
            part_id = f_b->data().part_id; 
          }

          auto& mesh = multisolid[part_id];

          int wall_label = 3; //inner wall
          if (!fp_a || !fp_b) wall_label = 2; //outer wall

          LinearRing wall_face_1;
          if ((h1a<h1b) and (h2a<h2b)) {
            wall_face_1.push_back(v2p(v1,h1b));
            // v1_other desc
            wall_face_1.insert(wall_face_1.end(), v1_other.rbegin(), v1_other.rend());
            wall_face_1.push_back(v2p(v1,h1a));
            wall_face_1.push_back(v2p(v2,h2a));
            // v2_other asc
            wall_face_1.insert(wall_face_1.end(), v2_other.begin(), v2_other.end());
            wall_face_1.push_back(v2p(v2,h2b));
            faces.push_back(wall_face_1);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_1, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
          } else 
          if ((h1a>h1b) and (h2a>h2b)) {
            wall_face_1.push_back(v2p(v2,h2a));
            // v2_other desc
            wall_face_1.insert(wall_face_1.end(), v2_other.rbegin(), v2_other.rend());
            wall_face_1.push_back(v2p(v2,h2b));
            wall_face_1.push_back(v2p(v1,h1b));
            // v1_other asc
            wall_face_1.insert(wall_face_1.end(), v1_other.begin(), v1_other.end());
            wall_face_1.push_back(v2p(v1,h1a));
            faces.push_back(wall_face_1);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_1, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
          } else 
          if ((h1a<h1b) and (h2a>h2b)) {
            // compute vx and hx
            auto l_a = EPECK::Line_3(EPECK::Point_3(p1.x(), p1.y(), h1a), EPECK::Point_3(p2.x(), p2.y(), h2a));
            auto l_b = EPECK::Line_3(EPECK::Point_3(p1.x(), p1.y(), h1b), EPECK::Point_3(p2.x(), p2.y(), h2b));
            auto result = CGAL::intersection(l_a, l_b);
            auto px = boost::get<typename EPECK::Point_3>(&*result);

            // TODO: check if distance from px to p1 and p2 is longer than snap_tolerance?

            extra_wall_points[edge] = *px;
            // EPECK::Point_2 px_2d(px->x(),px->y());
            // arr.split_edge(edge, AT::Segment_2(p1,px_2d), AT::Segment_2(px_2d,p2));
            // if (result) {
            // auto px = )
            // }

            wall_face_1.push_back(v2p(v1,h1b));
            // v1_other desc
            wall_face_1.insert(wall_face_1.end(), v1_other.rbegin(), v1_other.rend());
            wall_face_1.push_back(v2p(v1,h1a));
            wall_face_1.push_back(p2p(px));

            LinearRing wall_face_2;
            wall_face_2.push_back(v2p(v2,h2a));
            // v2_other desc
            wall_face_2.insert(wall_face_2.end(), v2_other.rbegin(), v2_other.rend());
            wall_face_2.push_back(v2p(v2,h2b));
            wall_face_2.push_back(p2p(px));
            
            faces.push_back(wall_face_1);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_1, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
            faces.push_back(wall_face_2);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_2, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
          } else
          if ((h1a>h1b) and (h2a<h2b)) {
            // compute vx and hx
            auto l_a = EPECK::Line_3(EPECK::Point_3(p1.x(), p1.y(), h1a), EPECK::Point_3(p2.x(), p2.y(), h2a));
            auto l_b = EPECK::Line_3(EPECK::Point_3(p1.x(), p1.y(), h1b), EPECK::Point_3(p2.x(), p2.y(), h2b));
            auto result = CGAL::intersection(l_a, l_b);
            auto px = boost::get<typename EPECK::Point_3>(&*result);

            // TODO: check if distance from px to p1 and p2 is longer than snap_tolerance?

            extra_wall_points[edge] = *px;
            // EPECK::Point_2 px_2d(px->x(),px->y());
            // arr.split_edge(edge, AT::Segment_2(p1,px_2d), AT::Segment_2(px_2d,p2));
            // if (result) {
              // auto px = )
            // }

            wall_face_1.push_back(v2p(v1,h1b));
            // v1_other asc
            wall_face_1.insert(wall_face_1.end(), v1_other.begin(), v1_other.end());
            wall_face_1.push_back(v2p(v1,h1a));
            wall_face_1.push_back(p2p(px));

            LinearRing wall_face_2;
            wall_face_2.push_back(v2p(v2,h2a));
            // v2_other asc
            wall_face_2.insert(wall_face_2.end(), v2_other.begin(), v2_other.end());
            wall_face_2.push_back(v2p(v2,h2b));
            wall_face_2.push_back(p2p(px));
            
            faces.push_back(wall_face_1);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_1, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
            faces.push_back(wall_face_2);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_2, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
          } else 
          if ((h1a>h1b) and (h2a==h2b)) {
            wall_face_1.push_back(v2p(v1,h1b));
            // v1_other asc
            wall_face_1.insert(wall_face_1.end(), v1_other.begin(), v1_other.end());
            wall_face_1.push_back(v2p(v1,h1a));
            wall_face_1.push_back(v2p(v2,h2a));
            faces.push_back(wall_face_1);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_1, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
          } else 
          if ((h1a<h1b) and (h2a==h2b)) {
            wall_face_1.push_back(v2p(v1,h1b));
            // v1_other desc
            wall_face_1.insert(wall_face_1.end(), v1_other.rbegin(), v1_other.rend());
            wall_face_1.push_back(v2p(v1,h1a));
            wall_face_1.push_back(v2p(v2,h2a));
            faces.push_back(wall_face_1);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_1, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
          } else
          if ((h2b>h2a) and (h1a==h1b)) {
            wall_face_1.push_back(v2p(v2,h2a));
            // v2_other asc
            wall_face_1.insert(wall_face_1.end(), v2_other.begin(), v2_other.end());
            wall_face_1.push_back(v2p(v2,h2b));
            wall_face_1.push_back(v2p(v1,h1a));
            faces.push_back(wall_face_1);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_1, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
          } else 
          if ((h2b<h2a) and (h1a==h1b)) {
            wall_face_1.push_back(v2p(v2,h2a));
            // v2_other desc
            wall_face_1.insert(wall_face_1.end(), v2_other.rbegin(), v2_other.rend());
            wall_face_1.push_back(v2p(v2,h2b));
            wall_face_1.push_back(v2p(v1,h1a));
            faces.push_back(wall_face_1);
            labels.push_back(wall_label);
            mesh.push_polygon(wall_face_1, wall_label);
            // mesh.push_attribute("surface_type", wall_label);
          }
        }
      }

      // roofs
      if (cfg.do_roofs) {
        for (auto face: arr.face_handles()) {
          if (face->data().in_footprint) {
            LinearRing roofpart;
            auto he = face->outer_ccb();
            
            push_ccb(roofpart, he, vertex_columns, extra_wall_points, snap_tolerance);

            for(Arrangement_2::Hole_iterator hole = face->holes_begin(); hole != face->holes_end(); ++hole ) {
              vec3f roofpart_hole;
              auto he = *hole;
              push_ccb(roofpart_hole, he, vertex_columns, extra_wall_points, snap_tolerance);
              if (roofpart_hole.size()>2) {
                roofpart.interior_rings().push_back(roofpart_hole);
              }
            }

            if (roofpart.size()>2) {
              faces.push_back(roofpart);
              labels.push_back(int(1));
              multisolid[face->data().part_id].push_polygon(roofpart, int(1));
              // mesh.push_attribute("surface_type", int(1));
            }

          }
        }

      }

      for (auto& mm : multisolid) {
        meshes.push_back(mm.second);
      }
      
    }

    /*
     * Convenience overload when base_elevation is passed
     */
    void compute(
        Arrangement_2& arr,
        float base_elevation,
        ArrangementExtruderConfig cfg
    ) override {
      auto const_elevation_provider_ptr = createElevationProvider(base_elevation);
      compute(arr, *const_elevation_provider_ptr, cfg);
    }

  };

  std::unique_ptr<ArrangementExtruderInterface> createArrangementExtruder() {
    return std::make_unique<ArrangementExtruder>();
  }
}