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

#include <CGAL/Cartesian.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/Search_traits_adapter.h>
#include <CGAL/linear_least_squares_fitting_3.h>
#include <CGAL/property_map.h>

#include <roofer/common/datastructures.hpp>
#include <unordered_map>

namespace roofer::linedect {
  typedef CGAL::Cartesian<double> SCK;

  typedef CGAL::Exact_predicates_inexact_constructions_kernel cgal_kernel;
  typedef cgal_kernel::Vector_3 Vector;
  typedef cgal_kernel::Point_3 Point;
  typedef cgal_kernel::Line_3 Line;

  using namespace std;

  typedef vector<vector<size_t>> NeighbourVec;

  class LineDetector {
    typedef std::pair<Point, size_t> point_index;
    typedef CGAL::Search_traits_3<cgal_kernel> Traits_base;
    typedef CGAL::Search_traits_adapter<
        point_index, CGAL::First_of_pair_property_map<point_index>, Traits_base>
        TreeTraits;
    typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
    typedef Neighbor_search::Tree Tree;

    typedef unordered_map<size_t, Line> SegShapes;
    typedef SegShapes::iterator SSIterator;

    vector<point_index> indexed_points;
    // Tree tree;
    // vector<bool> point_seed_flags;
    NeighbourVec neighbours;
    size_t region_counter = 1;

   public:
    vector<size_t>
        point_segment_idx;  // 0=unsegmented, maybe put this on the heap...
    SegShapes segment_shapes;
    int N = 5;
    double dist_thres = 0.2 * 0.2;
    size_t min_segment_count = 4;

    LineDetector(vector<Point> &points);
    LineDetector(vector<Point> &points, vector<vector<size_t>> neighbours);
    vector<size_t> get_point_indices(size_t shape_id);
    roofer::Segment project(const size_t i1, const size_t i2);
    SCK::Segment_3 project_cgal(const size_t i1, const size_t i2,
                                float extension);
    size_t get_bounded_edges(roofer::SegmentCollection &edges);
    std::vector<size_t> detect();

   private:
    inline Line fit_line(vector<size_t> &neighbour_idx);
    inline bool valid_candidate(Line &line, Point &p);
    bool grow_region(size_t seed_idx);
  };
}  // namespace roofer::linedect
