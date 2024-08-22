// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of geoflow-roofer (https://github.com/3DBAG/geoflow-roofer)

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
// Ivan Paden

#pragma once

#include <roofer/reconstruction/cdt_util.hpp>
#include <roofer/reconstruction/cgal_shared_definitions.hpp>

namespace roofer::reconstruction {

  struct ElevationProvider {
    virtual ~ElevationProvider() = default;

    virtual float get(const Point_2 pt) const = 0;

    virtual float get_percentile(float percentile) const = 0;
  };

  std::unique_ptr<ElevationProvider> createElevationProvider(
      const float floor_elevation);
  std::unique_ptr<ElevationProvider> createElevationProvider(
      const proj_tri_util::DT& base_cdt);

}  // namespace roofer::reconstruction
