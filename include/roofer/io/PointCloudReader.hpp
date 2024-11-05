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
#include <roofer/misc/projHelper.hpp>
#include <roofer/io/SpatialReferenceSystem.hpp>

namespace roofer::io {
  struct PointCloudReaderInterface {
    roofer::misc::projHelperInterface& pjHelper;

    PointCloudReaderInterface(roofer::misc::projHelperInterface& pjh)
        : pjHelper(pjh){};
    virtual ~PointCloudReaderInterface() = default;

    virtual void open(const std::string& source) = 0;

    virtual void get_crs(SpatialReferenceSystemInterface* srs) = 0;

    virtual void close() = 0;

    virtual TBox<double> getExtent() = 0;

    virtual void readPointCloud(PointCollection& points,
                                vec1i* classification = nullptr,
                                vec1i* order = nullptr,
                                vec1f* intensities = nullptr,
                                vec3f* colors = nullptr) = 0;
  };

  std::unique_ptr<PointCloudReaderInterface> createPointCloudReaderLASlib(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
