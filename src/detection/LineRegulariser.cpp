#include "LineRegulariser.hpp"
#include "LineRegulariserBase.hpp"

namespace roofer::detection {

  class LineRegulariser : public LineRegulariserInterface {

    void compute(
      const SegmentCollection& edge_segments,
      const SegmentCollection& ints_segments,
      LineRegulariserConfig cfg
    ) override {

      // get clusters from line regularisation 
      auto LR = linereg::LineRegulariser();
      LR.add_segments(0,edge_segments);
      LR.add_segments(2,ints_segments);
      LR.dist_threshold = cfg.dist_threshold*cfg.dist_threshold;
      LR.angle_threshold = cfg.angle_threshold;

      LR.perform_angle_clustering();
      LR.perform_distance_clustering();

      SegmentCollection edges_out_;
      vec1i priorities, angle_cluster_ids, dist_cluster_ids;
      // we should iterate of the distance clusters and output one segment per cluster
      for(auto& line : LR.lines) {
        linereg::Segment_2 segment;
        segment = line.segment;
        auto new_seg = Segment();
        new_seg[0] = {float(CGAL::to_double(segment.source().x())), float(CGAL::to_double(segment.source().y())), 0};
        new_seg[1] = {float(CGAL::to_double(segment.target().x())), float(CGAL::to_double(segment.target().y())), 0};
        edges_out_.push_back(new_seg);
        priorities.push_back(line.priority);
        priorities.push_back(line.priority);
        angle_cluster_ids.push_back(line.angle_cluster_id);
        angle_cluster_ids.push_back(line.angle_cluster_id);
        dist_cluster_ids.push_back(line.dist_cluster_id);
        dist_cluster_ids.push_back(line.dist_cluster_id);
      }
      for(auto& dclust : LR.dist_clusters) {
        size_t dclust_size = dclust->lines.size();
        linereg::Segment_2 segment;
        //get lines with highest priority
        size_t max_priority=0;
        for (auto line : dclust->lines) {
          if(line->priority > max_priority) max_priority = line->priority;
        }
        std::vector<linereg::linetype*> prio_lines;
        for (auto line : dclust->lines) {
          if(line->priority == max_priority) {
            prio_lines.push_back(line);
          }
        }
        // TODO: split clusters using common planes if intersection segment(s) are present? 
        // TODO: compute distance clusters with 3D dists? 
        // TODO: output exact segments? quick solve of spikes?
        // TODO: performance optimise clustering algo
        // //skip if cluster only contains fp segments
        // if(max_priority==1 && (dclust_size == prio_lines.size())) continue;
        if(!cfg.merge_intersection_lines && (max_priority==2 && (prio_lines.size()>1))) 
        {
          std::vector<linereg::linetype*> other_lines;
          for (auto line : dclust->lines) {
            if(line->priority != 2) {
              other_lines.push_back(line);
            }
          }
          for (auto line : prio_lines) {
            double mean_angle = line->angle;
            auto centroid = line->midpoint;
            segment = linereg::calc_segment(centroid, mean_angle, other_lines, cfg.extension);
            auto new_seg = Segment();
            new_seg[0] = {float(CGAL::to_double(segment.source().x())), float(CGAL::to_double(segment.source().y())), 0};
            new_seg[1] = {float(CGAL::to_double(segment.target().x())), float(CGAL::to_double(segment.target().y())), 0};
            regularised_edges.push_back(new_seg);
            exact_regularised_edges.push_back(segment);
          }
        } 
        else 
        {
          //compute mean line with small extensions on both ends
          double mean_angle = calc_mean_angle(prio_lines);
          auto centroid = calc_centroid(prio_lines);
          segment = linereg::calc_segment(centroid, mean_angle, dclust->lines, cfg.extension);
          auto new_seg = Segment();
          new_seg[0] = {float(CGAL::to_double(segment.source().x())), float(CGAL::to_double(segment.source().y())), 0};
          new_seg[1] = {float(CGAL::to_double(segment.target().x())), float(CGAL::to_double(segment.target().y())), 0};
          regularised_edges.push_back(new_seg);
          exact_regularised_edges.push_back(segment);
        }
      }
      // output("angle_cluster_id").set(angle_cluster_ids);
      // output("dist_cluster_id").set(dist_cluster_ids);
      // output("priorities").set(priorities);
      // output("edges_out_").set(edges_out_);
      // output("n_angle_clusters").set(int(LR.angle_clusters.size()));
      // output("edges_out").set(new_segments);
      // output("rings_out").set(new_rings);
      // output("footprint_out").set(new_fp);
    }
  };

  std::unique_ptr<LineRegulariserInterface> createLineRegulariser() {
    return std::make_unique<LineRegulariser>();
  };

}