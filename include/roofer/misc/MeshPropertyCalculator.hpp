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
#include <memory>
#include <roofer/common/datastructures.hpp>
#include <roofer/common/Raster.hpp>

namespace roofer::misc {

  struct ComputeRoofHeightConfig {
    float z_offset = 0;
    std::string h_50p = "h_50p";
    std::string h_70p = "h_70p";
    std::string h_min = "h_min";
    std::string h_max = "h_max";
  };

  struct ComputeRoofOrientationsConfig {
    std::string slope = "slope";
    std::string azimuth = "azimuth";
    std::string roof_type = "roof_type";
    float is_horizontal_threshold = 4;
  };

  struct MeshPropertyCalculatorInterface {
    virtual ~MeshPropertyCalculatorInterface() = default;
    virtual RasterTools::Raster get_heightmap(
        std::unordered_map<int, roofer::Mesh>& multisolid,
        const roofer::Box& box, float cellsize) = 0;
    virtual void calculate_h_attr(Mesh& mesh, RasterTools::Raster& r_lod22,
                                  ComputeRoofHeightConfig cfg) = 0;
    virtual void compute_roof_orientation(
        Mesh& mesh, ComputeRoofOrientationsConfig cfg) = 0;
  };

  std::unique_ptr<MeshPropertyCalculatorInterface>
  createMeshPropertyCalculator();
}  // namespace roofer::misc
