#pragma once

#include <vector>
#include <deque>
#include <random>
#include <algorithm>

#include <CGAL/property_map.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Search_traits_adapter.h>

#include "cgal_shared_definitions.hpp"

namespace roofer {

namespace regiongrower {

class CGAL_RegionGrowerDS {

  public:
  typedef std::pair<Point,size_t> point_index;
  typedef CGAL::Search_traits_3<EPICK>                       Traits_base;
  typedef CGAL::Search_traits_adapter<point_index,
  CGAL::First_of_pair_property_map<point_index>,
  Traits_base>                                              TreeTraits;
  typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
  typedef Neighbor_search::Tree Tree;

  typedef std::vector<std::vector<size_t>> vecvecui;

  roofer::PointCollection& points;
  vecvecui neighbours;
  size_t size;

  CGAL_RegionGrowerDS(roofer::PointCollection& points, size_t N=15)
    : points(points)
  {
    size = points.size();

    std::vector<point_index> indexed_points;
    indexed_points.reserve(size);

    size_t i=0;
    for(auto p: points)
      indexed_points.push_back(std::make_pair(Point(p[0], p[1], p[2]),i++));
    Tree tree;
    tree.insert(indexed_points.begin(), indexed_points.end());
    neighbours.resize(size);

    for(auto pi : indexed_points){
      auto p = pi.first;
      neighbours[pi.second].reserve(N);
      Neighbor_search search(tree, p, N+1);
      // auto gp = glm::make_vec3(points[pi.second].data());
      // skip the first point since it is identical to the query point
      for (auto neighbour = search.begin()+1 ; neighbour < search.end(); ++neighbour) {
        neighbours[pi.second].push_back(neighbour->first.second);
        // std::cout << pi.second << " : " << glm::distance(gp,glm::make_vec3(points[neighbour->first.second].data())) << "\n";
      }
    }
  };
  virtual std::deque<size_t> get_seeds() {
    std::deque<size_t> seeds;
    for (size_t i=0; i<size; ++i) {
      seeds.push_back(i);
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(seeds.begin(), seeds.end(), g);
    return seeds;
  }
  std::vector<size_t> get_neighbours(size_t idx) {
    return neighbours[idx];
  }
};

} // namespace regiongrower

} // namespace roofer

