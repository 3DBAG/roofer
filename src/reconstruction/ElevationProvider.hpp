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

#include "cgal_shared_definitions.hpp"
#include "cdt_util.hpp"

namespace roofer::detection {

  struct ElevationProvider {
    virtual ~ElevationProvider() = default;

    virtual float get(const Point_2 pt) const = 0;

    virtual float get_percentile(float percentile) const = 0;

  };

  std::unique_ptr<ElevationProvider> createElevationProvider(const float floor_elevation);
  std::unique_ptr<ElevationProvider> createElevationProvider(const proj_tri_util::DT& base_cdt);

} // namespace roofer::detection