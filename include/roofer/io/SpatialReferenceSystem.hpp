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
#include <string>

namespace roofer::io {
  struct SpatialReferenceSystemInterface {
    // std::string auth_name;
    // std::string code;
    // std::string wkt;
    virtual ~SpatialReferenceSystemInterface() = default;

    virtual bool is_valid() const = 0;
    virtual void clear() = 0;

    virtual void import(const std::string& user_input) = 0;
    virtual void import_epsg(const int epsg) = 0;
    virtual void import_wkt(const std::string& wkt) = 0;
    virtual std::string export_wkt() const = 0;

    virtual std::string get_auth_name() const = 0;
    virtual std::string get_auth_code() const = 0;
  };

  std::unique_ptr<SpatialReferenceSystemInterface>
  createSpatialReferenceSystemOGR();
}  // namespace roofer::io
