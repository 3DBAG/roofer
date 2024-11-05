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
#include <roofer/common/Raster.hpp>
#include <roofer/common/datastructures.hpp>
#include <roofer/reconstruction/cgal_shared_definitions.hpp>

namespace roofer::reconstruction {

  struct SegmentRasteriserConfig {
    float cellsize = 0.05;
    float thres_alpha = 0.25;
    bool use_ground = true;
    int megapixel_limit = 600;
    bool fill_nodata_ = true;
    int fill_nodata_window_size_ = 5;
  };

  struct SegmentRasteriserInterface {
    RasterTools::Raster heightfield;

    // add_vector_input("triangles", typeid(TriangleCollection));
    // add_vector_input("ground_triangles", typeid(TriangleCollection));
    // add_vector_input("alpha_rings", typeid(LinearRing));
    // add_input("roofplane_ids", typeid(vec1i));
    // add_input("pts_per_roofplane", typeid(IndexedPlanesWithPoints));
    // add_output("heightfield", typeid(RasterTools::Raster));

    // add_output("heightfield", typeid(RasterTools::Raster));
    // add_output("grid_points", typeid(PointCollection));
    // add_output("values", typeid(vec1f));
    // add_output("data_area", typeid(float));

    virtual ~SegmentRasteriserInterface() = default;
    virtual void compute(
        TriangleCollection& roof_triangles,
        TriangleCollection& ground_triangles,
        SegmentRasteriserConfig config = SegmentRasteriserConfig()) = 0;
  };

  std::unique_ptr<SegmentRasteriserInterface> createSegmentRasteriser();
}  // namespace roofer::reconstruction
