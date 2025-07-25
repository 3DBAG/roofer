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

#include <roofer/common/CGALPIPTester.hpp>

namespace roofer {

  CGALPIPTester::CGALPIPTester(const LinearRing& polygon) {
    // Convert exterior ring to CGAL polygon
    Polygon_2 exterior;
    for (const auto& pt : polygon) {
      exterior.push_back(Point_2(pt[0], pt[1]));
    }

    // Convert interior rings (holes) to CGAL polygons
    std::vector<Polygon_2> holes;
    for (const auto& hole : polygon.interior_rings()) {
      Polygon_2 hole_poly;
      for (const auto& pt : hole) {
        hole_poly.push_back(Point_2(pt[0], pt[1]));
      }
      holes.push_back(hole_poly);
    }

    // Create polygon with holes
    polygon_with_holes = std::make_unique<Polygon_with_holes_2>(exterior, holes.begin(), holes.end());
  }

  bool CGALPIPTester::test(const arr3f& p) {
    Point_2 query_point(p[0], p[1]);

    // First check if point is inside the outer boundary
    auto outer_result = polygon_with_holes->outer_boundary().bounded_side(query_point);

    // If point is outside the outer boundary, it's definitely outside
    if (outer_result == CGAL::ON_UNBOUNDED_SIDE) {
      return false;
    }

    // If point is on the boundary of outer polygon, consider it inside
    if (outer_result == CGAL::ON_BOUNDARY) {
      return true;
    }

    // Point is inside outer boundary, now check holes
    for (auto hole_it = polygon_with_holes->holes_begin();
         hole_it != polygon_with_holes->holes_end(); ++hole_it) {
      auto hole_result = hole_it->bounded_side(query_point);

      // If point is inside any hole, it's outside the polygon
      if (hole_result == CGAL::ON_BOUNDED_SIDE) {
        return false;
      }

      // If point is on the boundary of a hole, consider it outside
      if (hole_result == CGAL::ON_BOUNDARY) {
        return false;
      }
    }

    // Point is inside outer boundary and not inside any hole
    return true;
  }

}  // namespace roofer
