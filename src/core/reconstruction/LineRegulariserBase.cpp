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
#include "LineRegulariserBase.hpp"
#include <iterator>

namespace linereg {

  double calc_mean_angle(const std::vector<linetype*>& lines) {
    // length-weighted mean angle
    double angle_sum=0, sqlenth_sum=0;
    for (auto& line : lines) {
      sqlenth_sum += line->sqlength;
    }
    for (auto& line : lines) {
      angle_sum += line->angle * line->sqlength;
    }
    return angle_sum/sqlenth_sum;
  }

  Point_2 calc_centroid(const std::vector<linetype*>& lines) {
    double cx=0, cy=0;
    for (auto& line : lines) {
      auto p = line->segment.source();
      cx += CGAL::to_double(p.x());
      cy += CGAL::to_double(p.y());
      p = line->segment.target();
      cx += CGAL::to_double(p.x());
      cy += CGAL::to_double(p.y());
    }
    size_t np = 2*lines.size();
    return Point_2(cx/np, cy/np);
  }

  // construct a line L with centroid and mean_angle, then project all the segments from lines on L to bound it to a segment
  Segment_2 calc_segment(Point_2 centroid, double mean_angle, const std::vector<linetype*>& lines, double extension) {
    auto lv = Vector_2(std::tan(mean_angle), 1.0);
    lv = lv / CGAL::sqrt(CGAL::to_double(lv.squared_length()));

    bool setminmax=false;
    Point_2 pmin, pmax;
    double dmin, dmax;
    for (auto& line : lines) {
      auto p = line->segment.source();
      auto d = CGAL::to_double(Vector_2(p.x(),p.y())*lv);
      if (!setminmax) {
        setminmax=true;
        dmin=dmax=d;
        pmin=pmax=p;
      }
      if (d < dmin){
        dmin = d;
        pmin = p;
      }
      if (d > dmax) {
        dmax = d;
        pmax = p;
      }

      p = line->segment.target();
      d = CGAL::to_double(Vector_2(p.x(),p.y())*lv);
      if (d < dmin){
        dmin = d;
        pmin = p;
      }
      if (d > dmax) {
        dmax = d;
        pmax = p;
      }
    }
    Line_2 l(centroid, lv);
    pmin = l.projection(pmin) - lv*extension;
    pmax = l.projection(pmax) + lv*extension;
    return Segment_2(pmin, pmax);
  }

  double AngleCluster::distance(Cluster<double>* other_cluster) {
    return std::fabs(value - other_cluster->value);
  }
  void AngleCluster::calc_mean_value() {
    value = calc_mean_angle(lines);
  }

  double DistCluster::distance(Cluster<Segment_2>* other_cluster) {
    return CGAL::to_double(CGAL::squared_distance(value, other_cluster->value));
  }
  void DistCluster::calc_mean_value() {
    // a segment through the length-weighted mean centroid and elongated to 'cover' all the segments
    double mean_angle = calc_mean_angle(lines);
    Point_2 centroid = calc_centroid(lines);
    value = calc_segment(centroid, mean_angle, lines);
  }

  template <typename ClusterH> DistanceTable<ClusterH>::DistanceTable(std::set<ClusterH>& clusters) : clusters(clusters) {
    // compute only half of the distance table, since the other half will be exactly the same
    for(auto cit_a = clusters.begin(); cit_a != clusters.end(); ++cit_a) {
      for(auto cit_b = std::next(cit_a); cit_b != clusters.end(); ++cit_b) {
        auto cluster_a = *cit_a;
        auto cluster_b = *cit_b;
        // do not create entries for cluster pairs that both have an intersection line, these are not to be merged
        // if (cluster_a->has_intersection_line && cluster_b->has_intersection_line) continue;
        // push distance to heap
        auto hhandle = distances.push(ClusterPairDist(
          std::make_pair(cluster_a, cluster_b),
          cluster_a->distance(cluster_b.get())
        ));
        // store handle for both clusters
        auto hh = std::make_shared<heap_handle>(hhandle);
        cluster_to_dist_pairs[cluster_a].push_back(hh);
        cluster_to_dist_pairs[cluster_b].push_back(hh);
      }
    }
  }
  template <typename ClusterH> void DistanceTable<ClusterH>::merge(ClusterH lhs, ClusterH rhs) {
    // merges two clusters, then removes one from the distances map and update the affected distances
    // merge
    lhs->lines.insert(lhs->lines.end(), rhs->lines.begin(), rhs->lines.end());
    // lhs->has_intersection_line = lhs->has_intersection_line || rhs->has_intersection_line;
    lhs->calc_mean_value();

    // iterate distancetable
    // if rhs in pair: remove pair
    // if lhs has intersection line and lhs in a pair: remove pair if the other cluster also has an intersection line. Since clusters that both have intersection line are not te be merged.
    auto& rhs_hhandles = cluster_to_dist_pairs[rhs];
    for (auto& hhandle : cluster_to_dist_pairs[rhs]) {
      if (hhandle.use_count()==2) {
        distances.erase(*hhandle);
      }
    }
    clusters.erase(rhs);
    cluster_to_dist_pairs.erase(rhs);

    auto& lhs_hhandles = cluster_to_dist_pairs[lhs];
    auto i = lhs_hhandles.begin();
    while (i != lhs_hhandles.end()) {
      // we check the use count of the shared ptr to our heap handle. There should be exactly 2 for the case where both clusters still exist, otherwise one has been merged before and the heap handle was also erased before
      if (i->use_count()!=2)
      {
        lhs_hhandles.erase(i++);
      } else {
        // dereference 3 times! iterator->shared_ptr->heap_handle->heap_value Notice -> is not implemented on the heap_handle, so we must use * for the last dereference here
        (***i).dist = (***i).clusters.first->distance((***i).clusters.second.get());
        distances.update(*(*i));
        ++i;
      }
    }
  }
  template <typename ClusterH> typename DistanceTable<ClusterH>::ClusterPairDist DistanceTable<ClusterH>::get_closest_pair() {
    ClusterPairDist min_pair = distances.top(); //distances.pop(); <- do not pop here; the node will be removed later in the merge function!
    return min_pair;
  }

  template class DistanceTable<AngleClusterH>;
  template class DistanceTable<DistClusterH>;

  void LineRegulariser::perform_angle_clustering() {
    //make clusters
    angle_clusters.clear();
    for(auto& line : lines) {
      auto aclusterh = std::make_shared<AngleCluster>();
      aclusterh->value = line.angle;
      // aclusterh->has_intersection_line = line.priority==2;
      aclusterh->lines.push_back(&line);
      angle_clusters.insert(aclusterh);
    }

    if (angle_clusters.size()>1) {
      // make distance table
      DistanceTable adt(angle_clusters);

      auto apair = adt.get_closest_pair();
      while (apair.dist < angle_threshold) {
        adt.merge(apair.clusters.first, apair.clusters.second);
        if (adt.distances.size()==0) break;
        apair = adt.get_closest_pair();
      }
    }

    size_t id_cntr=0;
    for(auto& aclusterh : angle_clusters) {
      for(auto line : aclusterh->lines){
        line->angle_cluster = aclusterh;
        line->angle_cluster_id = id_cntr;
      }
      ++id_cntr;
    }
  }
  void LineRegulariser::perform_distance_clustering() {
    dist_clusters.clear();

    // perform distance clustering for each angle cluster
    for(auto& aclusterh : angle_clusters) {
      std::set<DistClusterH> dclusters;
      for(auto& line : aclusterh->lines) {
        auto dclusterh = std::make_shared<DistCluster>();
        dclusterh->value = line->segment;
        // dclusterh->has_intersection_line = line->priority==2;
        dclusterh->lines.push_back(line);
        dclusters.insert(dclusterh);
      }
      
      if (dclusters.size()>1) {
        // make distance table
        DistanceTable ddt(dclusters);

        // do clustering
        auto dpair = ddt.get_closest_pair();
        while (dpair.dist < dist_threshold) {
          ddt.merge(dpair.clusters.first, dpair.clusters.second);
          if (ddt.distances.size()==0) break;
          dpair = ddt.get_closest_pair();
        }
      }
      dist_clusters.insert(dclusters.begin(), dclusters.end());
    }

    size_t id_cntr=0;
    for(auto& dclusterh : dist_clusters) {
      for(auto line : dclusterh->lines) {
        line->dist_cluster = dclusterh;
        line->dist_cluster_id = id_cntr;
      }
      ++id_cntr;
    }
  }
}