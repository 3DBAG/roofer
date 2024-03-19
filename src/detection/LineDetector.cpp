#include "LineDetector.hpp"
#include <set>
#include "LineDetectorBase.h"
#include "LineRegulariserBase.hpp"

namespace roofer::detection {

  typedef std::pair<size_t,size_t> IDPair;
  struct Cmp {
    bool operator()(const IDPair& lhs, const IDPair& rhs) const { 
        return lhs.first < rhs.first; 
    }
  };
  // typedef std::map<IDPair, size_t, Cmp> RingSegMap;

  inline size_t detect_lines_ring(linedect::LineDetector& LD, const Plane& plane, SegmentCollection& segments_out, roofer::detection::LineDetectorConfig& cfg) {
    LD.dist_thres = cfg.dist_thres * cfg.dist_thres;
    LD.N = cfg.k;
    auto& c_upper = cfg.min_cnt_range.second;
    auto& c_lower = cfg.min_cnt_range.first;
    for (size_t i=c_upper; i>=c_lower; --i){
      LD.min_segment_count = i;
      LD.detect();
    }
    size_t ringsize = LD.point_segment_idx.size();
              // chain the detected lines, to ensure correct order
    if (LD.segment_shapes.size()>1) {
      std::vector<std::pair<size_t,size_t> > new_ring_ids;
      bool start_seg = LD.point_segment_idx[0];
      int prev_i=ringsize-1,
        prev_seg=LD.point_segment_idx[prev_i], 
        cur_seg, 
        i_last_seg = -1;
      bool perfect_aligned=false; // is the first point of the ring also the first point of a segment? If yes, we are perfectly aligned!
      for( int i=0; i<ringsize; ++i ) {
        cur_seg = LD.point_segment_idx[i];
        if(cur_seg==prev_seg && cur_seg!=0) { // we are inside a segment

        } else if (cur_seg!=0 && prev_seg==0) { // from unsegmented to segmented
          new_ring_ids.push_back(std::make_pair(i, cur_seg)); // first of cur
          if(i==0) perfect_aligned=true;
          // new_ring_ids.push_back(i); // end of unsegmented linesegment
        } else if (cur_seg!=0 && prev_seg!=0) { // from one segment to another
          new_ring_ids.push_back(std::make_pair(prev_i, prev_seg)); // last of prev
          new_ring_ids.push_back(std::make_pair(i, cur_seg)); // first of cur
        } else if (cur_seg==0 && prev_seg!=0) { //from segment to unsegmented
          new_ring_ids.push_back(std::make_pair(prev_i, prev_seg)); // last of prev
          // new_ring_ids.push_back(prev_i); // begin of unsegmented linesegment
        } // else: we are inside an unsegmented or segmented zone
        prev_seg = cur_seg;
        prev_i = i;
      }
      if (!perfect_aligned) { // add the segment that runs through the first point in the original ring
        new_ring_ids.insert(new_ring_ids.begin(), new_ring_ids.back());
        new_ring_ids.pop_back();
      }
      //ensure the ring is aligned wrt diff region ids around origin of the ring
      if (new_ring_ids.front().second == new_ring_ids.back().second) {
        size_t region = new_ring_ids.front().second;
        do {
          new_ring_ids.push_back(new_ring_ids.front());
          new_ring_ids.erase(new_ring_ids.begin());
        } while (region == new_ring_ids.front().second);
      }
      //merge multiple segments of the same region
      std::unordered_map<size_t,std::pair<size_t,size_t> > map_by_region;
      for (auto el = new_ring_ids.begin(); el<new_ring_ids.end(); ++el) {
        if(!map_by_region.count(el->second)) {
          map_by_region[el->second] = std::make_pair(el->first, el->first);
        } else {
          map_by_region[el->second].second = el->first;
        }
      }
      //sort the segments acc to order in ring
      typedef std::set<std::pair<size_t,size_t>,Cmp> SegSet;
      SegSet sorted_segments;
      for (auto& el : map_by_region) {
        sorted_segments.insert(el.second);
      }
      // TODO: better check for overlapping segments! Following is not 100% robust...
      if (cfg.remove_overlap) {
        auto el_prev = sorted_segments.begin();
        // el_prev.first-=sorted_segments.size();
        // el_prev.second-=sorted_segments.size();
        std::vector<SegSet::iterator> to_remove;
        for (auto el = ++sorted_segments.begin(); el != sorted_segments.end(); ++el ){
          el_prev = el;
          --el_prev;
          if (el_prev->second > el->first)
            to_remove.push_back(el);
        }
        for (auto el : to_remove) {
          sorted_segments.erase(el);
        }
      }
      if (cfg.perform_chaining) {
        std::vector<SCK::Segment_3> prechain_segments;
        std::vector<size_t> idx; size_t idcnt=0;
        for (auto& [i0,i1] : sorted_segments) {
          // segments_out.push_back( LD.project(i0, i1) );
          prechain_segments.push_back( LD.project_cgal(i0, i1, cfg.line_extend) );
          idx.push_back(idcnt++);
        }
        // TODO: chain the ring? for better regularisation results
        SegmentCollection new_ring;
        auto chained_ring_pts = linereg::chain_ring<SCK>(idx, SCK::Plane_3(plane.a(), plane.b(), plane.c(), plane.d()), prechain_segments, cfg.snap_threshold, cfg.line_extend);

        if (chained_ring_pts.size() > 2) {
          auto first = chained_ring_pts.begin();
          for (auto pit=std::next(first); pit!=chained_ring_pts.end(); ++pit) {
            auto p2 = *pit;
            auto p1 = *std::prev(pit);
            segments_out.push_back({
              arr3f{
                float(CGAL::to_double(p1.x())),
                float(CGAL::to_double(p1.y())),
                float(CGAL::to_double(p1.z()))},
              arr3f{
                float(CGAL::to_double(p2.x())),
                float(CGAL::to_double(p2.y())),
                float(CGAL::to_double(p2.z()))},
            });
          }
          auto p1 = *chained_ring_pts.rbegin();
          auto p2 = *first;
          segments_out.push_back({
            arr3f{
              float(CGAL::to_double(p1.x())),
              float(CGAL::to_double(p1.y())),
              float(CGAL::to_double(p1.z()))},
            arr3f{
              float(CGAL::to_double(p2.x())),
              float(CGAL::to_double(p2.y())),
              float(CGAL::to_double(p2.z()))},
          });
        }

        return segments_out.size();
      } else {
        for (const auto& e : sorted_segments) {
          segments_out.push_back(
            LD.project(e.first,e.second)
          );
        }
        return sorted_segments.size();
      }
    } else return 0;
    
  }

  class LineDetector : public LineDetectorInterface {
    public:
    using LineDetectorInterface::LineDetectorInterface;

    void detect(
      const std::vector<LinearRing>& edge_points,
      const vec1i& roofplane_ids,
      const IndexedPlanesWithPoints& pts_per_roofplane,
      LineDetectorConfig cfg
    ) override {     
      SegmentCollection lines3d;
      vec1i ring_order, ring_id, is_start;
      std::unordered_map<size_t,std::vector<size_t>> ring_idx;

      int n = cfg.k;
      
      size_t seg_cntr=0, plane_id;
      for (size_t i=0; i<edge_points.size(); ++i) {
        auto& ring = edge_points[i];
        plane_id = roofplane_ids[i];
        std::vector<linedect::Point> cgal_pts;
        for( auto& p : ring ) {
          cgal_pts.push_back(linedect::Point(p[0], p[1], p[2]));
        }

        linedect::LineDetector LD(cgal_pts);
        // SegmentCollection ring_edges;
        auto n_detected = detect_lines_ring(LD, pts_per_roofplane.at(plane_id).first, edge_segments, cfg);
        LD.get_bounded_edges(lines3d);

        for (size_t j=0; j<n_detected; ++j) {
          // edge_segments.push_back(ring_edges[j]);
          ring_idx[plane_id].push_back(seg_cntr++);
          ring_order.push_back(j);
          ring_id.push_back(plane_id);
          ring_order.push_back(j);
          ring_id.push_back(plane_id);
          is_start.push_back(1);
          is_start.push_back(0);
        }

        // also check the holes
        for (auto& hole : ring.interior_rings()) {
          std::vector<linedect::Point> cgal_pts;
          for( auto& p : hole ) {
            cgal_pts.push_back(linedect::Point(p[0], p[1], p[2]));
          }

          linedect::LineDetector LD(cgal_pts);
          // SegmentCollection ring_edges;
          auto n_detected = detect_lines_ring(LD, pts_per_roofplane.at(plane_id).first, edge_segments, cfg);
          LD.get_bounded_edges(lines3d);

          for (size_t j=0; j<n_detected; ++j) {
            // edge_segments.push_back(ring_edges[j]);
            ring_idx[plane_id].push_back(seg_cntr++);
            ring_order.push_back(j);
            ring_id.push_back(plane_id);
            ring_order.push_back(j);
            ring_id.push_back(plane_id);
            is_start.push_back(1);
            is_start.push_back(0);
          }
        }
        // std::cout << "number of shapes: " << LD.segment_shapes.size() <<"\n";
        // std::cout << "number of segments: " << order_cnt <<"\n";
      }
      // }

      // output("edge_segments").set(edge_segments);
      // output("lines3d").set(lines3d);
      // // output("ring_edges").set(ring_edges);
      // output("ring_idx").set(ring_idx);
      // output("ring_id").set(ring_id);
      // output("ring_order").set(ring_order);
      // output("is_start").set(is_start);
    }
  };

  std::unique_ptr<LineDetectorInterface> createLineDetector() {
    return std::make_unique<LineDetector>();
  };

}