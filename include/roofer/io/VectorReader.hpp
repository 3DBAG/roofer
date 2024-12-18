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
#include <optional>
#include <roofer/common/datastructures.hpp>
#include <roofer/io/SpatialReferenceSystem.hpp>
#include <roofer/misc/projHelper.hpp>

namespace roofer::io {
  struct VectorReaderInterface {
    roofer::misc::projHelperInterface& pjHelper;
    std::optional<roofer::TBox<double>> region_of_interest;
    roofer::TBox<double> layer_extent;

    int layer_id = 0;
    std::string layer_name = "";
    std::string attribute_filter = "";

    VectorReaderInterface(roofer::misc::projHelperInterface& pjh)
        : pjHelper(pjh){};
    virtual ~VectorReaderInterface() = default;

    virtual void open(const std::string& source) = 0;

    virtual size_t get_feature_count() = 0;

    virtual void get_crs(roofer::io::SpatialReferenceSystemInterface* srs) = 0;

    virtual void readPolygons(std::vector<LinearRing>&,
                              AttributeVecMap* attributes = nullptr) = 0;
  };

  std::unique_ptr<VectorReaderInterface> createVectorReaderOGR(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
