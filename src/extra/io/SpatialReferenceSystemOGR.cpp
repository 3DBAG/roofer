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

#include <roofer/logger/logger.h>
#include <roofer/io/SpatialReferenceSystem.hpp>
#include <string>
#include <ogr_spatialref.h>

namespace roofer::io {

  struct SpatialReferenceSystemOGR : public SpatialReferenceSystemInterface {
    OGRSpatialReference srs;

    void import(const std::string& user_input) override {
      srs.SetFromUserInput(user_input.c_str());
    };

    void import_epsg(const int epsg) override { srs.importFromEPSG(epsg); };

    void import_wkt(const std::string& wkt) override {
      auto c_str = wkt.c_str();
      srs.importFromWkt(&c_str);
    };

    std::string export_wkt() const override {
      char** wkt_ptr;
      srs.exportToWkt(wkt_ptr);
      std::string wkt = *wkt_ptr;
      return wkt;
    };

    bool is_valid() const override { return srs.Validate() == OGRERR_NONE; };

    void clear() override { srs.Clear(); };

    std::string get_auth_name() const override {
      return srs.GetAuthorityName(nullptr);
    };

    std::string get_auth_code() const override {
      return srs.GetAuthorityCode(nullptr);
    };
  };

  std::unique_ptr<SpatialReferenceSystemInterface>
  createSpatialReferenceSystemOGR() {
    return std::make_unique<SpatialReferenceSystemOGR>();
  };
};  // namespace roofer::io
