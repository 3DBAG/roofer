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

namespace roofer::io {
  struct RasterWriterInterface {
    roofer::misc::projHelperInterface& pjHelper;

    RasterWriterInterface(roofer::misc::projHelperInterface& pjh)
        : pjHelper(pjh){};
    virtual ~RasterWriterInterface() = default;

    virtual void writeBands(const std::string& source, ImageMap& bands) = 0;
  };

  std::unique_ptr<RasterWriterInterface> createRasterWriterGDAL(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
