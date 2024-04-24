#include "ArrangementBase.hpp"
#include "ArrangementDissolver.hpp"

namespace roofer::detection {
  
  class ArrangementDissolver : public ArrangementDissolverInterface{

    public:
    void compute(
        Arrangement_2& arr,
        const RasterTools::Raster& heightfield,
        ArrangementDissolverConfig cfg
      ) override {
      
      if(cfg.dissolve_seg_edges) {
        Face_merge_observer obs(arr);
        arr_dissolve_seg_edges(arr);
      }
      
      if (cfg.dissolve_all_interior) {
        arr_dissolve_fp(arr, true, false);
      }

      if (cfg.dissolve_step_edges) {
        // compute elevations so we can do perform the generalisation lod2.2 -> lod1.3
        for (auto face: arr.face_handles()) {
          if (face->data().in_footprint) {
            LinearRing polygon;
            arrangementface_to_polygon(face, polygon);

            auto height_points = heightfield.rasterise_polygon(polygon, true);
            size_t datasize = height_points.size(), data_cnt = 0;
            if(datasize==0) { //polygon was too small to yield any pixels/height_points
              auto& pz = polygon[0][2];
              face->data().elevation_50p = pz;
              face->data().elevation_70p = pz;
              face->data().elevation_97p = pz;
              face->data().elevation_min = pz;
              face->data().elevation_max = pz;
              face->data().data_coverage = 0;
            } else {
              auto& plane = face->data().plane;
              // fall back to elevation based on the plane of this face. This is more reliable in case the data_coverage is limited
              for (auto& p : height_points) {
                if(p[2]!=heightfield.noDataVal_) 
                  data_cnt++;
                else
                  p[2] = -plane.a()/plane.c() * p[0] - plane.b()/plane.c()*p[1] - plane.d()/plane.c();
              }
              std::sort(height_points.begin(), height_points.end(), [](auto& p1, auto& p2) {
                return p1[2] < p2[2];
              });
              int elevation_id = std::floor(0.5*float(datasize-1));
              face->data().elevation_50p = height_points[elevation_id][2];
              elevation_id = std::floor(0.7*float(datasize-1));
              face->data().elevation_70p = height_points[elevation_id][2];
              elevation_id = std::floor(0.97*float(datasize-1));
              face->data().elevation_97p = height_points[elevation_id][2];
              face->data().elevation_min = height_points[0][2];
              face->data().elevation_max = height_points[datasize-1][2];
              face->data().data_coverage = float(data_cnt) / float(datasize);
            }
            face->data().pixel_count = datasize;
          }
        }
      
        Face_merge_observer obs(arr);
        arr_dissolve_step_edges(arr, cfg.step_height_threshold);
      }

      // remove any dangling edges
      {
        std::vector<Arrangement_2::Halfedge_handle> to_remove;
        for (auto he : arr.edge_handles()) {
          if (he->face()==he->twin()->face())
            to_remove.push_back(he);
        }
        for (auto he : to_remove) {
          arr.remove_edge(he);
        }
      }

      // remove all lines not inside footprint
      if (cfg.dissolve_outside_fp) {
        arr_dissolve_fp(arr, false, true);
      }

      arr_remove_redundant_vertices(arr);

      // label different building parts
      arr_label_buildingparts(arr);
      
      // ensure all holes are marked as such
      auto f_unb = arr.unbounded_face();
      for (auto fh : arr.face_handles()) {
        if(fh!=f_unb) 
          if(!fh->data().in_footprint && !fh->data().is_footprint_hole) {
            fh->data().is_footprint_hole = true;
          }
      }

      // if (remove_duplicates) {
      //   arr_snap_duplicates(arr, (double) std::pow(10,-dupe_threshold_exp));
      // }
      // snap edge shorter than snap_tolerance
      // for (auto he : arr.edge_handles()) {
      //   if(CGAL::squared_distance(he->source()->point(), he->target()->point()) < snap_tolerance) {

      //     arr.remove_edge(he->next());
      //     arr.remove_edge(he->twin()->previous());
      //   }
      // }
      // compute final data_area and elevation stats for each face
      std::vector<float> all_elevations;
      for (auto face: arr.face_handles()) {
        if (face->data().in_footprint) {
          LinearRing polygon;
          arrangementface_to_polygon(face, polygon);

          auto height_points = heightfield.rasterise_polygon(polygon, true);
          size_t data_cnt = 0;
          bool skip_face = height_points.size()==0;
          {
            // auto& plane = face->data().plane;
            // compute elevations based on the assigned plane on this face
            std::vector<arr3f*> pts;
            if (!skip_face){
              for (auto& p : height_points) {
                if(p[2]!=heightfield.noDataVal_) {
                  data_cnt++;
                  pts.push_back(&p);
                  all_elevations.push_back(p[2]);
                };
              }
            }
            if(data_cnt==0) {
              auto& pz = polygon[0][2];
              face->data().elevation_50p = heightfield.noDataVal_;
              face->data().elevation_70p = heightfield.noDataVal_;
              face->data().elevation_97p = heightfield.noDataVal_;
              face->data().elevation_min = heightfield.noDataVal_;
              face->data().elevation_max = heightfield.noDataVal_;
              face->data().data_coverage = 0;
            } else {
              std::sort(pts.begin(), pts.end(), [](auto& p1, auto& p2) {
                return (*p1)[2] < (*p2)[2];
              });
              size_t datasize = pts.size();
              int elevation_id = std::floor(0.5*float(datasize-1));
              face->data().elevation_50p = (*(pts[elevation_id])) [2];
              elevation_id = std::floor(0.7*float(datasize-1));
              face->data().elevation_70p = (*(pts[elevation_id])) [2];
              elevation_id = std::floor(0.97*float(datasize-1));
              face->data().elevation_97p = (*(pts[elevation_id])) [2];
              face->data().elevation_min = (*(pts[0])) [2];
              face->data().elevation_max = (*(pts[datasize-1])) [2];
              face->data().data_coverage = float(data_cnt) / float(height_points.size());
            }
            face->data().pixel_count = data_cnt;
          }
        }
      }

      // compute golbal height statistics
      // if (all_elevations.size()==0) {
      //   output("global_elevation_50p").set_from_any(std::any());
      //   output("global_elevation_70p").set_from_any(std::any());
      //   output("global_elevation_97p").set_from_any(std::any());
      //   output("global_elevation_min").set_from_any(std::any());
      //   output("global_elevation_max").set_from_any(std::any());  
      // } else {
      //   std::sort(all_elevations.begin(), all_elevations.end());
      //   size_t last_id = all_elevations.size()-1;
      //   int elevation_id = std::floor(0.5*float(last_id));
      //   output("global_elevation_50p").set(all_elevations[elevation_id]);
      //   elevation_id = std::floor(0.7*float(last_id));
      //   output("global_elevation_70p").set(all_elevations[elevation_id]);
      //   elevation_id = std::floor(0.99*float(last_id));
      //   output("global_elevation_97p").set(all_elevations[elevation_id]);
      //   output("global_elevation_min").set(all_elevations[0]);
      //   output("global_elevation_max").set(all_elevations[last_id]);
      // }
      // output("arrangement").set(arr);
    }
  };

  std::unique_ptr<ArrangementDissolverInterface> createArrangementDissolver() {
    return std::make_unique<ArrangementDissolver>();
  }

}