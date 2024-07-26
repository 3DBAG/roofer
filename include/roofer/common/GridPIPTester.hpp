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
#pragma once

#include <roofer/common/ptinpoly.h>

#include <roofer/common/common.hpp>
#include <roofer/common/datastructures.hpp>
#include <vector>

namespace roofer {

  class GridPIPTester {
    pGridSet ext_gridset;
    std::vector<pGridSet> hole_gridsets;
    int Grid_Resolution = 20;

    public:
    GridPIPTester(const LinearRing& polygon);
    GridPIPTester(const GridPIPTester&) = delete;
    ~GridPIPTester();
    bool test(const arr3f& p);
  };
  
}  // namespace roofer
