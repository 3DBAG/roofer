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
#include <cstddef>
#include <memory>
#include <roofer/common/datastructures.hpp>
#include <roofer/misc/projHelper.hpp>
#include <roofer/io/SpatialReferenceSystem.hpp>

namespace roofer::io {
  struct VectorWriterInterface {
    std::string conn_string_ = "out";
    std::string gdaldriver_ = "GPKG";
    std::string layername_ = "geom";
    bool overwrite_layer_ = true;
    bool overwrite_file_ = true;
    bool create_directories_ = true;
    bool do_transactions_ = false;
    int transaction_batch_size_ = 1000;

    roofer::misc::projHelperInterface& pjHelper;

    VectorWriterInterface(roofer::misc::projHelperInterface& pjh)
        : pjHelper(pjh){};
    virtual ~VectorWriterInterface() = default;

    virtual void writePolygons(const std::string& source,
                               const SpatialReferenceSystemInterface* srs,
                               const std::vector<LinearRing>& polygons,
                               const AttributeVecMap& attributes, size_t begin,
                               size_t end) = 0;

    void writePolygons(const std::string& source,
                       const SpatialReferenceSystemInterface* srs,
                       const std::vector<LinearRing>& polygons,
                       const AttributeVecMap& attributes) {
      writePolygons(source, srs, polygons, attributes, 0, polygons.size());
    };
  };

  std::unique_ptr<VectorWriterInterface> createVectorWriterOGR(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
