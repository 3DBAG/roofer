// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ravi Peters

#pragma once

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/Search_traits_adapter.h>
#include <CGAL/property_map.h>

#include <algorithm>
#include <deque>
#include <random>
#include <roofer/common/common.hpp>
#include <roofer/reconstruction/cgal_shared_definitions.hpp>
#include <vector>

namespace roofer {

  namespace regiongrower {

    class CGAL_RegionGrowerDS {
     public:
      typedef std::pair<Point, size_t> point_index;
      typedef CGAL::Search_traits_3<EPICK> Traits_base;
      typedef CGAL::Search_traits_adapter<
          point_index, CGAL::First_of_pair_property_map<point_index>,
          Traits_base>
          TreeTraits;
      typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
      typedef Neighbor_search::Tree Tree;

      typedef std::vector<std::vector<size_t>> vecvecui;

      roofer::PointCollection& points;
      vecvecui neighbours;
      size_t size;

      CGAL_RegionGrowerDS(roofer::PointCollection& points, size_t N = 15)
          : points(points) {
        size = points.size();

        std::vector<point_index> indexed_points;
        indexed_points.reserve(size);

        size_t i = 0;
        for (auto p : points)
          indexed_points.push_back(
              std::make_pair(Point(p[0], p[1], p[2]), i++));
        Tree tree;
        tree.insert(indexed_points.begin(), indexed_points.end());
        neighbours.resize(size);

        for (auto pi : indexed_points) {
          auto p = pi.first;
          neighbours[pi.second].reserve(N);
          Neighbor_search search(tree, p, N + 1);
          // auto gp = glm::make_vec3(points[pi.second].data());
          // skip the first point since it is identical to the query point
          for (auto neighbour = search.begin() + 1; neighbour < search.end();
               ++neighbour) {
            neighbours[pi.second].push_back(neighbour->first.second);
            // std::cout << pi.second << " : " <<
            // glm::distance(gp,glm::make_vec3(points[neighbour->first.second].data()))
            // << "\n";
          }
        }
      };
      virtual std::deque<size_t> get_seeds() {
        std::deque<size_t> seeds;
        for (size_t i = 0; i < size; ++i) {
          seeds.push_back(i);
        }
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(seeds.begin(), seeds.end(), g);
        return seeds;
      }
      std::vector<size_t> get_neighbours(size_t idx) { return neighbours[idx]; }
    };

  }  // namespace regiongrower

}  // namespace roofer
