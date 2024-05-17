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

#include <queue>
#include <stack>
#include "LineDetectorBase.h"

using namespace std;
using namespace linedect;

LineDetector::LineDetector(vector<Point> &points) {
  auto size = points.size();
  indexed_points.reserve(size);
  size_t i=0;
  for(auto p: points)
    indexed_points.push_back(std::make_pair(p,i++));
  point_segment_idx.resize(size, 0);
  // point_seed_flags.resize(size, true);
  Tree tree;
  tree.insert(indexed_points.begin(), indexed_points.end());
  neighbours.resize(size);
  for(auto pi : indexed_points){
    auto p = pi.first;
    neighbours[pi.second].reserve(N);
    Neighbor_search search(tree, p, N);
    for (auto neighbour : search) {
      neighbours[pi.second].push_back(neighbour.first.second);
    }
  }
}

LineDetector::LineDetector(vector<Point> &points, vector<vector<size_t>> neighbours):neighbours(neighbours) {
  auto size = points.size();
  indexed_points.reserve(size);
  size_t i=0;
  for(auto p: points)
    indexed_points.push_back(std::make_pair(p,i++));
  point_segment_idx.resize(size, 0);
  // point_seed_flags.resize(size, true);
}

vector<size_t> LineDetector::get_point_indices(size_t shape_id) {
  vector<size_t> result;
  for (auto pi:indexed_points){
    auto idx = pi.second;
    if (point_segment_idx[idx] == shape_id)
      result.push_back(idx);
  }
  return result;
}

roofer::Segment LineDetector::project(const size_t i1, const size_t i2) {
  const auto& l = segment_shapes[point_segment_idx[i1]];
  const auto& p1 = indexed_points[i1].first;
  const auto& p2 = indexed_points[i2].first;
  auto p1n = l.projection(p1);
  auto p2n = l.projection(p2);
  return roofer::Segment(
    roofer::arr3f{float(p1n.x()), float(p1n.y()), float(p1n.z())},
    roofer::arr3f{float(p2n.x()), float(p2n.y()), float(p2n.z())}
  );
}
SCK::Segment_3 LineDetector::project_cgal(const size_t i1, const size_t i2, float extension) {
  const auto& l = segment_shapes[point_segment_idx[i1]];
  const auto& p1 = indexed_points[i1].first;
  const auto& p2 = indexed_points[i2].first;
  auto p1n = l.projection(p1);
  auto p2n = l.projection(p2);
  // extend the linesegment a bit in both directions
  auto v = (p2n-p1n);
  v = v/CGAL::sqrt(v.squared_length());
  p1n = p1n - v*extension;
  p2n = p2n + v*extension;
  return SCK::Segment_3(
    SCK::Point_3(CGAL::to_double(p1n.x()), CGAL::to_double(p1n.y()), CGAL::to_double(p1n.z())),
    SCK::Point_3(CGAL::to_double(p2n.x()), CGAL::to_double(p2n.y()), CGAL::to_double(p2n.z()))
  );
}

size_t LineDetector::get_bounded_edges(roofer::SegmentCollection& edges) {
  std::vector<size_t> id_mins;
  std::map<size_t, roofer::Segment> ordered_segments;
  for(auto seg: segment_shapes){
    auto l = seg.second;
    auto l_idx = get_point_indices(seg.first);
    // std::cout << l.direction() << ", #Pts: " << l_idx.size() <<std::endl;
    //find the two extreme points on this line
    double minpl=1, maxpl=0;
    size_t minpl_id, maxpl_id;
    size_t j=0, id_min=l_idx.size();
    auto point_on_line = l.point(0);
    auto l_normal = l.to_vector()/CGAL::sqrt(l.to_vector().squared_length());
    for(auto id : l_idx){
      // edges_index_map[id] = line_id;
      // project this_p on l
      linedect::Point this_p(indexed_points[id].first);
      const Vector a(point_on_line, this_p);
      double pl = a*l_normal;

      if (j++==0){
        minpl = maxpl = pl;
        minpl_id = maxpl_id = id;
      } else {
        if (pl < minpl) {
          minpl=pl;
          minpl_id = id;
        }  else if (pl > maxpl) {
          maxpl=pl;
          maxpl_id = id;
        }
      }
      // keep track of lowest point id
      id_min = std::min(id_min, id);
    }
    Point p0,p1;
    if (minpl_id < maxpl_id) {
      p0 = (point_on_line + minpl*l_normal);
      p1 = (point_on_line + maxpl*l_normal);
    } else {
      p1 = (point_on_line + minpl*l_normal);
      p0 = (point_on_line + maxpl*l_normal);
    }
    ordered_segments[id_min] = roofer::Segment();
    ordered_segments[id_min][0] = {
      float(p0.x()),
      float(p0.y()),
      float(p0.z())
    };
    ordered_segments[id_min][1] = {
      float(p1.x()),
      float(p1.y()),
      float(p1.z())
    };
  }
  // deliver the edges in order
  for (auto& kv : ordered_segments) {
    edges.push_back(kv.second);
  }
  return ordered_segments.size();
}
inline Line LineDetector::fit_line(vector<size_t>& neighbour_idx){
  vector<Point> neighbor_points;
  for (auto neighbor_id: neighbour_idx){
    neighbor_points.push_back(indexed_points[neighbor_id].first);
  }
  Line line;
  linear_least_squares_fitting_3(neighbor_points.begin(),neighbor_points.end(),line,CGAL::Dimension_tag<0>());
  return line;
}

std::vector<size_t> LineDetector::detect(){
  // seed generation
  typedef pair<size_t,double> index_dist_pair;
  auto cmp = [](index_dist_pair left, index_dist_pair right) {return left.second < right.second;};
  priority_queue<index_dist_pair, vector<index_dist_pair>, decltype(cmp)> pq(cmp);

  // size_t i=0;
  for(auto pi : indexed_points){
    if (point_segment_idx[pi.second]==0) {
      auto p = pi.first;
      auto line = fit_line(neighbours[pi.second]);
      auto line_dist = CGAL::squared_distance(line, p);
      pq.push(index_dist_pair(pi.second, line_dist));
    }
  }

  std::vector<size_t> new_regions;

  // region growing from seed points
  while(pq.size()>0) {
    auto idx = pq.top().first; pq.pop();
    // if (point_seed_flags[idx]){
    if (point_segment_idx[idx]==0){
      if (grow_region(idx))
        new_regions.push_back(region_counter);
      region_counter++;
    }
  }
  return new_regions;
}

inline bool LineDetector::valid_candidate(Line &line, Point &p) {
  return CGAL::squared_distance(line, p) < dist_thres;
}

bool LineDetector::grow_region(size_t seed_idx) {
  auto p = indexed_points[seed_idx];
  auto line = fit_line(neighbours[seed_idx]);
  segment_shapes[region_counter] = line;

  vector<Point> points_in_region;
  vector<size_t> idx_in_region;
  stack<size_t> candidates;
  candidates.push(seed_idx);
  point_segment_idx[seed_idx] = region_counter;
  points_in_region.push_back(p.first); 
  idx_in_region.push_back(p.second); 
  
  while (candidates.size()>0){
    auto candidate_id = candidates.top(); candidates.pop();
    auto cp = indexed_points[candidate_id].first;
    for (auto n_id: neighbours[candidate_id]){
      if (point_segment_idx[n_id]!=0)
        continue;
      // point_seed_flags[n_id] = false; // this point can no longer be used as seed
      if (valid_candidate(segment_shapes[region_counter], indexed_points[n_id].first)){
        point_segment_idx[n_id] = region_counter;
        candidates.push(n_id);
        points_in_region.push_back(indexed_points[n_id].first);
        idx_in_region.push_back(n_id);
        Line line;
        linear_least_squares_fitting_3(points_in_region.begin(),points_in_region.end(),line,CGAL::Dimension_tag<0>());
        segment_shapes[region_counter] = line;
      }

    }
  }
  // undo region if it doesn't satisfy quality criteria
  if (points_in_region.size()<min_segment_count){
    segment_shapes.erase(region_counter);
    for (auto idx: idx_in_region)
      point_segment_idx[idx] = 0;
    return false;
  } return true;
}
