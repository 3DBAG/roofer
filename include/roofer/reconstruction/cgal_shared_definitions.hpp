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

#include <CGAL/Arr_extended_dcel.h>
#include <CGAL/Arr_linear_traits_2.h>
#include <CGAL/Arrangement_2.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

namespace roofer {

  typedef CGAL::Exact_predicates_exact_constructions_kernel EPECK;
  typedef CGAL::Exact_predicates_inexact_constructions_kernel EPICK;
  typedef EPICK::Point_3 Point;
  typedef EPICK::Vector_3 Vector;
  typedef EPICK::Vector_2 Vector_2;
  typedef EPICK::Plane_3 Plane;
  typedef EPICK::Line_3 Line;

  typedef std::unordered_map<int, std::pair<Plane, std::vector<Point>>>
      IndexedPlanesWithPoints;

  // Arrangment definitions

  typedef CGAL::Arr_linear_traits_2<EPECK> Traits_2;
  typedef Traits_2::Segment_2 Segment_2;
  typedef Traits_2::Point_2 Point_2;

  struct FaceInfo {
    bool is_finite = false;
    bool is_ground = false;
    bool in_footprint = false;
    bool is_footprint_hole = false;
    float elevation_50p = 0;
    float elevation_70p = 0;
    float elevation_97p = 0;
    float elevation_min = 0, elevation_max = 0;
    float data_coverage = 0;
    int pixel_count = 0;
    int segid = 0;
    int part_id = -1;
    float rms_error_to_avg = 0;

    Plane plane;
    std::vector<Point> points;

    // graph-cut optimisation
    size_t label = 0;
    size_t v_index;
    std::vector<double> vertex_label_cost;
  };
  struct EdgeInfo {
    bool blocks = false;

    // graph-cut optimisation
    double edge_weight;
  };
  typedef CGAL::Arr_extended_dcel<Traits_2, bool, EdgeInfo, FaceInfo> Dcel;
  typedef CGAL::Arrangement_2<Traits_2, Dcel> Arrangement_2;

}  // namespace roofer
