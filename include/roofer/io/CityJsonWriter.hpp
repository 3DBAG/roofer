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
#include <ostream>
#include <memory>
#include <roofer/common/datastructures.hpp>
#include <roofer/misc/projHelper.hpp>
#include <roofer/io/SpatialReferenceSystem.hpp>

namespace roofer::io {

  struct CityJSONMetadataProperties {
    std::string poc_name;
    std::string poc_emailAddress;
    std::string poc_phone;
    std::string poc_contactType;
    std::string poc_contactName;
    std::string poc_website;
    std::string title;
    std::string identifier;
    std::string referenceDate;
  };

  struct CityJsonWriterInterface {
    // parameter variables
    std::string CRS_ = "EPSG:7415";
    std::string filepath_;
    std::string identifier_attribute = "";
    size_t written_features_count = 0;

    bool prettyPrint_ = false;

    vec1s key_options;

    double translate_x_ = 0.;
    double translate_y_ = 0.;
    double translate_z_ = 0.;
    double scale_x_ = 0.01;
    double scale_y_ = 0.01;
    double scale_z_ = 0.01;

    roofer::misc::projHelperInterface& pjHelper;

    CityJsonWriterInterface(roofer::misc::projHelperInterface& pjh)
        : pjHelper(pjh){};
    virtual ~CityJsonWriterInterface() = default;

    // write metadata
    // write features

    virtual void write_metadata(std::ostream& output_stream,
                                const SpatialReferenceSystemInterface* srs,
                                const roofer::TBox<double>& extent,
                                CityJSONMetadataProperties props) = 0;
    virtual void write_feature(
        std::ostream& output_stream, const LinearRing& footprint,
        const std::unordered_map<int, Mesh>* geometry_lod12,
        const std::unordered_map<int, Mesh>* geometry_lod13,
        const std::unordered_map<int, Mesh>* geometry_lod22,
        const AttributeMapRow attributes) = 0;

    // virtual void write(const std::string& source, const LinearRing&
    // footprints,
    //                    const std::unordered_map<int, Mesh>* geometry_lod12,
    //                    const std::unordered_map<int, Mesh>* geometry_lod13,
    //                    const std::unordered_map<int, Mesh>* geometry_lod22,
    //                    const AttributeVecMap& attributes,
    //                    const size_t attribute_index) = 0;
  };

  std::unique_ptr<CityJsonWriterInterface> createCityJsonWriter(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
