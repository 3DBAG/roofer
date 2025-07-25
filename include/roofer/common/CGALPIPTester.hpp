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

#include <roofer/common/common.hpp>
#include <roofer/common/datastructures.hpp>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>

#include <vector>
#include <memory>

namespace roofer {

  class CGALPIPTester {
   private:
    using K = CGAL::Exact_predicates_inexact_constructions_kernel;
    using Point_2 = K::Point_2;
    using Polygon_2 = CGAL::Polygon_2<K>;
    using Polygon_with_holes_2 = CGAL::Polygon_with_holes_2<K>;

    std::unique_ptr<Polygon_with_holes_2> polygon_with_holes;

   public:
    CGALPIPTester(const LinearRing& polygon);
    CGALPIPTester(const CGALPIPTester&) = delete;
    CGALPIPTester& operator=(const CGALPIPTester&) = delete;
    ~CGALPIPTester() = default;

    bool test(const arr3f& p);
  };

}  // namespace roofer
