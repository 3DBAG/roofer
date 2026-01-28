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
#include <string>
#include <optional>
#include <roofer/io/CityJsonWriter.hpp>
#include <nlohmann/json.hpp>

// Include the cxx-generated header for Rust FFI types
// The header is copied to build/include/roofer-fcb-ffi/src/lib.rs.h during build
#include "roofer-fcb-ffi/src/lib.rs.h"

// The generated header defines FcbWriterHandle in the global namespace
// and functions in the global namespace (not in a namespace)
// We'll use them directly with :: prefix

namespace roofer::io {

  class FcbWriter : public CityJsonWriterInterface {
    std::optional<::rust::Box<::FcbWriterHandle>> rust_writer_;
    std::string current_file_path_;
    bool metadata_written_ = false;
    uint64_t expected_feature_count_ = 0;
    std::string attribute_schema_json_ = "{}";

    // Helper to convert CityJSON structure to JSON string
    std::string cityjson_to_string(const nlohmann::json& cj);

   public:
    explicit FcbWriter(roofer::misc::projHelperInterface& pjh);
    ~FcbWriter() override;
    
    // Close current FCB file (for split mode)
    void close_current_file();
    void set_expected_feature_count(uint64_t count) { expected_feature_count_ = count; }
    // Set attribute schema JSON (built from all features) to avoid per-feature own_schema overhead
    void set_attribute_schema_json(const std::string& schema_json) { attribute_schema_json_ = schema_json; }

    void write_metadata(std::ostream& output_stream,
                        const SpatialReferenceSystemInterface* srs,
                        const roofer::TBox<double>& extent,
                        CityJSONMetadataProperties props) override;

    void write_feature(
        std::ostream& output_stream, const LinearRing& footprint,
        const std::unordered_map<int, Mesh>* geometry_lod12,
        const std::unordered_map<int, Mesh>* geometry_lod13,
        const std::unordered_map<int, Mesh>* geometry_lod22,
        const AttributeMapRow attributes) override;
  };

  std::unique_ptr<CityJsonWriterInterface> createFcbWriter(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
