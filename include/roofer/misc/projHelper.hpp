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
// Ravi Peters

#pragma once
#include <memory>
#include <roofer/common/common.hpp>
#include <roofer/common/datastructures.hpp>
#include <roofer/io/SpatialReferenceSystem.hpp>

namespace roofer::misc {

  struct projHelperInterface {
    std::optional<arr3d> data_offset;
    std::unique_ptr<roofer::io::SpatialReferenceSystemInterface> srs;

    projHelperInterface()
        : srs(roofer::io::createSpatialReferenceSystemOGR()){};
    virtual ~projHelperInterface() = default;

    // virtual void proj_construct() = 0;
    // virtual void proj_clone_from(const projHelperInterface&) = 0;
    virtual void proj_clear() = 0;

    virtual arr3f coord_transform_fwd(const double& x, const double& y,
                                      const double& z) = 0;
    virtual arr3d coord_transform_rev(const float& x, const float& y,
                                      const float& z) = 0;
    virtual arr3d coord_transform_rev(const arr3f& p) = 0;

    // virtual void set_process_crs(const char* crs) = 0;
    // virtual void set_fwd_crs_transform(
    // const char* source_crs, bool normalize_for_visualization = false) = 0;
    // virtual void set_rev_crs_transform(
    // const char* target_crs, bool normalize_for_visualization = false) = 0;
    // virtual const roofer::io::SpatialReferenceSystemInterface& crs() = 0;
    // virtual void set_crs(const roofer::io::SpatialReferenceSystemInterface&
    // crs) = 0; virtual void clear_fwd_crs_transform() = 0; virtual void
    // clear_rev_crs_transform() = 0;

    virtual void set_data_offset(arr3d& offset) = 0;
  };

  std::unique_ptr<projHelperInterface> createProjHelper();
}  // namespace roofer::misc
