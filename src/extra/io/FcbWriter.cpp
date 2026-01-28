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
// Balazs Dukai

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <roofer/io/FcbWriter.hpp>
#include <roofer/common/datastructures.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <optional>

// Include the cxx-generated header (generated during Rust build)
// The header is copied to build/include/roofer-fcb-ffi/src/lib.rs.h during build
#include "roofer-fcb-ffi/src/lib.rs.h"

namespace roofer::io {

  // Forward declaration
  nlohmann::json::object_t attributes2json_fcb(const AttributeMapRow& attributes);

  namespace fs = std::filesystem;

  // Helper methods (duplicated from CityJsonWriter to avoid modifying it)
  template <typename T>
  void add_vertices_ring_fcb(roofer::misc::projHelperInterface& pjHelper,
                              std::map<arr3d, size_t>& vertex_map,
                              std::vector<arr3d>& vertex_vec,
                              std::set<arr3d>& vertex_set, const T& ring,
                              TBox<double>& bbox) {
    size_t v_cntr = vertex_vec.size();
    for (auto& vertex_ : ring) {
      auto vertex = pjHelper.coord_transform_rev(vertex_);
      bbox.add(vertex);
      auto [it, did_insert] = vertex_set.insert(vertex);
      if (did_insert) {
        vertex_map[vertex] = v_cntr++;
        vertex_vec.push_back(vertex);
      }
    }
  }

  TBox<double> add_vertices_polygon_fcb(roofer::misc::projHelperInterface& pjHelper,
                                        std::map<arr3d, size_t>& vertex_map,
                                        std::vector<arr3d>& vertex_vec,
                                        std::set<arr3d>& vertex_set,
                                        const LinearRing& polygon) {
    TBox<double> bbox;
    add_vertices_ring_fcb(pjHelper, vertex_map, vertex_vec, vertex_set, polygon, bbox);
    for (auto& iring : polygon.interior_rings()) {
      add_vertices_ring_fcb(pjHelper, vertex_map, vertex_vec, vertex_set, iring, bbox);
    }
    return bbox;
  }

  TBox<double> add_vertices_mesh_fcb(roofer::misc::projHelperInterface& pjHelper,
                                     std::map<arr3d, size_t>& vertex_map,
                                     std::vector<arr3d>& vertex_vec,
                                     std::set<arr3d>& vertex_set,
                                     const Mesh& mesh) {
    TBox<double> bbox;
    for (auto& face : mesh.get_polygons()) {
      bbox.add(add_vertices_polygon_fcb(pjHelper, vertex_map, vertex_vec, vertex_set, face));
    }
    return bbox;
  }

  std::vector<std::vector<size_t>> LinearRing2jboundary_fcb(
      roofer::misc::projHelperInterface& pjHelper,
      std::map<arr3d, size_t>& vertex_map, const LinearRing& face) {
    std::vector<std::vector<size_t>> jface;
    std::vector<size_t> exterior_ring;
    for (auto& vertex_ : face) {
      auto vertex = pjHelper.coord_transform_rev(vertex_);
      exterior_ring.push_back(vertex_map[vertex]);
    }
    jface.emplace_back(std::move(exterior_ring));
    for (auto& iring : face.interior_rings()) {
      std::vector<size_t> interior_ring;
      for (auto& vertex_ : iring) {
        auto vertex = pjHelper.coord_transform_rev(vertex_);
        interior_ring.push_back(vertex_map[vertex]);
      }
      jface.emplace_back(std::move(interior_ring));
    }
    return jface;
  }

  nlohmann::json::object_t mesh2jSolid_fcb(const Mesh& mesh, const char* lod,
                                           std::map<arr3d, size_t>& vertex_map,
                                           roofer::misc::projHelperInterface& pjHelper) {
    auto geometry = nlohmann::json::object();
    geometry["type"] = "Solid";
    geometry["lod"] = lod;
    std::vector<std::vector<std::vector<size_t>>> exterior_shell;

    for (auto& face : mesh.get_polygons()) {
      exterior_shell.emplace_back(LinearRing2jboundary_fcb(pjHelper, vertex_map, face));
    }
    geometry["boundaries"] = {exterior_shell};

    auto semantic_objects = nlohmann::json::array();
    semantic_objects.push_back(nlohmann::json::object({{"type", "GroundSurface"}}));
    semantic_objects.push_back(nlohmann::json::object(
        {{"type", "WallSurface"}, {"on_footprint_edge", true}}));
    semantic_objects.push_back(nlohmann::json::object(
        {{"type", "WallSurface"}, {"on_footprint_edge", false}}));

    std::vector<int> sem_values;
    size_t wallSurface_cntr = semantic_objects.size();
    for (size_t i = 0; i < mesh.get_polygons().size(); ++i) {
      auto& label = mesh.get_labels()[i];
      if (label == 0) {
        sem_values.push_back(0);
      } else if (label == 1) {
        nlohmann::json::object_t semantic_object;
        if (mesh.get_attributes().size()) {
          semantic_object = attributes2json_fcb(mesh.get_attributes().at(i));
        }
        semantic_object["type"] = "RoofSurface";
        semantic_objects.push_back(semantic_object);
        sem_values.push_back(wallSurface_cntr++);
      } else if (label == 2) {
        sem_values.push_back(1);
      } else if (label == 3) {
        sem_values.push_back(2);
      } else {
        throw rooferException("Unknown label in mesh");
      }
    }
    geometry["semantics"] = {{"surfaces", semantic_objects}, {"values", {sem_values}}};
    return geometry;
  }

  nlohmann::json::array_t compute_geographical_extent_fcb(const TBox<double>& bbox) {
    auto minp = bbox.min();
    auto maxp = bbox.max();
    return {minp[0], minp[1], minp[2], maxp[0], maxp[1], maxp[2]};
  }

  nlohmann::json::object_t attributes2json_fcb(const AttributeMapRow& attributes) {
    nlohmann::json::object_t jattributes;
    nlohmann::json j_null;
    for (const auto& [name, val] : attributes) {
      if (attributes.is_null(name)) {
        jattributes[name] = j_null;
      } else if (auto val = attributes.get_if<bool>(name)) {
        jattributes[name] = *val;
      } else if (auto val = attributes.get_if<float>(name)) {
        jattributes[name] = *val;
      } else if (auto val = attributes.get_if<int>(name)) {
        jattributes[name] = *val;
      } else if (auto val = attributes.get_if<std::string>(name)) {
        jattributes[name] = *val;
      } else if (auto val = attributes.get_if<Date>(name)) {
        auto t = *val;
        std::string date = t.format_to_ietf();
        jattributes[name] = date;
      } else if (auto val = attributes.get_if<Time>(name)) {
        auto t = *val;
        std::string time = std::to_string(t.hour) + ":" + std::to_string(t.minute) + ":" +
                          std::to_string(t.second) + "Z";
        jattributes[name] = time;
      } else if (auto val = attributes.get_if<DateTime>(name)) {
        auto t = *val;
        std::string datetime = t.format_to_ietf();
        jattributes[name] = datetime;
      }
    }
    return jattributes;
  }

  using MeshMap = std::unordered_map<int, Mesh>;
  void write_cityobject_fcb(const LinearRing& footprint, const MeshMap* multisolid_lod12,
                            const MeshMap* multisolid_lod13, const MeshMap* multisolid_lod22,
                            const AttributeMapRow& attributes, nlohmann::json& outputJSON,
                            std::vector<arr3d>& vertex_vec, std::string& identifier_attribute,
                            std::string building_id, roofer::misc::projHelperInterface& pjHelper) {
    std::map<arr3d, size_t> vertex_map;
    std::set<arr3d> vertex_set;

    bool export_lod12 = multisolid_lod12;
    bool export_lod13 = multisolid_lod13;
    bool export_lod22 = multisolid_lod22;

    {
      auto building = nlohmann::json::object();
      auto b_id = building_id;
      building["type"] = "Building";
      building["attributes"] = attributes2json_fcb(attributes);
      for (const auto& [name, val] : attributes) {
        if (name == identifier_attribute) {
          if (auto val = attributes.get_if<float>(name)) {
            b_id = std::to_string(*val);
          } else if (auto val = attributes.get_if<int>(name)) {
            b_id = std::to_string(*val);
          } else if (auto val = attributes.get_if<std::string>(name)) {
            b_id = *val;
          }
        }
      }

      auto fp_geometry = nlohmann::json::object();
      fp_geometry["lod"] = "0";
      fp_geometry["type"] = "MultiSurface";
      add_vertices_polygon_fcb(pjHelper, vertex_map, vertex_vec, vertex_set, footprint);
      fp_geometry["boundaries"] = {LinearRing2jboundary_fcb(pjHelper, vertex_map, footprint)};
      building["geometry"].push_back(fp_geometry);

      std::vector<std::string> buildingPartIds;
      bool has_solids = false;
      if (export_lod12) has_solids = multisolid_lod12->size();
      if (export_lod13) has_solids = multisolid_lod13->size();
      if (export_lod22) has_solids = multisolid_lod22->size();

      TBox<double> building_bbox;
      if (has_solids) {
        const MeshMap* meshmap;
        if (export_lod22) {
          meshmap = multisolid_lod22;
        } else if (export_lod13) {
          meshmap = multisolid_lod13;
        } else if (export_lod12) {
          meshmap = multisolid_lod12;
        }
        for (const auto& [sid, solid_lodx] : *meshmap) {
          auto buildingPart = nlohmann::json::object();
          auto bp_id = b_id + "-" + std::to_string(sid);
          buildingPartIds.push_back(bp_id);
          buildingPart["type"] = "BuildingPart";
          buildingPart["parents"] = {b_id};

          if (export_lod12) {
            try {
              building_bbox = add_vertices_mesh_fcb(pjHelper, vertex_map, vertex_vec,
                                                    vertex_set, multisolid_lod12->at(sid));
              buildingPart["geometry"].push_back(
                  mesh2jSolid_fcb(multisolid_lod12->at(sid), "1.2", vertex_map, pjHelper));
            } catch (const std::exception& e) {
              // skip
            }
          }
          if (export_lod13) {
            try {
              building_bbox = add_vertices_mesh_fcb(pjHelper, vertex_map, vertex_vec,
                                                    vertex_set, multisolid_lod13->at(sid));
              buildingPart["geometry"].push_back(
                  mesh2jSolid_fcb(multisolid_lod13->at(sid), "1.3", vertex_map, pjHelper));
            } catch (const std::exception& e) {
              // skip
            }
          }
          if (export_lod22) {
            building_bbox = add_vertices_mesh_fcb(pjHelper, vertex_map, vertex_vec, vertex_set,
                                                  multisolid_lod22->at(sid));
            buildingPart["geometry"].push_back(
                mesh2jSolid_fcb(multisolid_lod22->at(sid), "2.2", vertex_map, pjHelper));
          }
          outputJSON["CityObjects"][bp_id] = buildingPart;
        }
      }
      building["children"] = buildingPartIds;
      building["geographicalExtent"] = compute_geographical_extent_fcb(building_bbox);
      outputJSON["CityObjects"][b_id] = building;
    }
  }

  // FcbWriter implementation
  FcbWriter::FcbWriter(roofer::misc::projHelperInterface& pjh)
      : CityJsonWriterInterface(pjh), metadata_written_(false) {}

  FcbWriter::~FcbWriter() {
    if (rust_writer_.has_value()) {
      try {
        ::close_file(**rust_writer_);
        rust_writer_.reset();
      } catch (...) {
        // Ignore errors during cleanup
      }
    }
  }

  std::string FcbWriter::cityjson_to_string(const nlohmann::json& cj) {
    return cj.dump();
  }

  void FcbWriter::write_metadata(std::ostream& output_stream,
                                 const SpatialReferenceSystemInterface* srs,
                                 const roofer::TBox<double>& extent,
                                 CityJSONMetadataProperties props) {
    // Build CityJSON metadata structure (same as CityJsonWriter)
    nlohmann::json outputJSON;
    outputJSON["type"] = "CityJSON";
    outputJSON["version"] = "2.0";
    outputJSON["CityObjects"] = nlohmann::json::object();
    outputJSON["vertices"] = nlohmann::json::array();

    outputJSON["transform"] = {{"scale", {scale_x_, scale_y_, scale_z_}},
                               {"translate", {translate_x_, translate_y_, translate_z_}}};

    auto metadata = nlohmann::json::object();
    metadata["geographicalExtent"] = compute_geographical_extent_fcb(extent);

    if (props.identifier.size()) metadata["identifier"] = props.identifier;

    auto contact = nlohmann::json::object();
    if (props.poc_contactName.size()) contact["contactName"] = props.poc_contactName;
    if (props.poc_emailAddress.size()) contact["emailAddress"] = props.poc_emailAddress;
    if (props.poc_phone.size()) contact["phone"] = props.poc_phone;
    if (props.poc_contactType.size()) contact["contactType"] = props.poc_contactType;
    if (props.poc_website.size()) contact["website"] = props.poc_website;
    if (contact.size()) metadata["pointOfContact"] = contact;

    if (props.referenceDate.empty()) {
      auto t = std::time(nullptr);
      auto tm = *std::localtime(&t);
      std::ostringstream oss;
      oss << std::put_time(&tm, "%Y-%m-%d");
      props.referenceDate = oss.str();
    }
    metadata["referenceDate"] = props.referenceDate;

    if (srs->is_valid()) {
      metadata["referenceSystem"] = "https://www.opengis.net/def/crs/" +
                                    srs->get_auth_name() + "/0/" + srs->get_auth_code();
    }

    if (props.title.size()) metadata["title"] = props.title;
    if (metadata.size()) outputJSON["metadata"] = metadata;

    // For FCB, we need the actual file path
    // Use filepath_ member if set, otherwise try to get from current_file_path_
    std::string file_path;
    if (!filepath_.empty()) {
      file_path = filepath_;
    } else if (!current_file_path_.empty()) {
      file_path = current_file_path_;
    } else {
      throw rooferException("FCB writer: file path must be set in filepath_ before write_metadata");
    }
    
    // Change extension from .city.jsonl to .fcb
    if (file_path.size() >= 11 && file_path.substr(file_path.size() - 11) == ".city.jsonl") {
      file_path = file_path.substr(0, file_path.length() - 11) + ".fcb";
    } else if (file_path.size() >= 6 && file_path.substr(file_path.size() - 6) == ".jsonl") {
      file_path = file_path.substr(0, file_path.length() - 6) + ".fcb";
    } else {
      file_path += ".fcb";
    }
    
    // Store for later use
    current_file_path_ = file_path;

    // Create Rust writer
    // The generated header defines functions in global namespace
    rust_writer_ = ::create_fcb_writer();

    // Serialize to JSON string and pass to Rust
    std::string json_str = cityjson_to_string(outputJSON);
    try {
      // Functions take FcbWriterHandle& (reference), so double-dereference: optional->Box->Handle
      // Use const char* constructor which should be implemented
      ::rust::Str json_str_rust(json_str.c_str());
      ::rust::Str file_path_rust(file_path.c_str());
      if (expected_feature_count_ == 0) {
        throw rooferException(
            "FCB writer: expected_feature_count_ is 0. "
            "Set it from the serializer before calling write_metadata.");
      }
      // Use the attribute schema JSON (built from all features by the serializer)
      ::rust::Str attr_schema_json_rust(attribute_schema_json_.c_str());
      ::write_metadata(**rust_writer_, json_str_rust, file_path_rust,
                       static_cast<uint64_t>(expected_feature_count_),
                       attr_schema_json_rust);
    } catch (const std::exception& e) {
      throw rooferException("Failed to write FCB metadata: " + std::string(e.what()));
    }

    metadata_written_ = true;
  }

  void FcbWriter::write_feature(std::ostream& output_stream, const LinearRing& footprint,
                                const std::unordered_map<int, Mesh>* geometry_lod12,
                                const std::unordered_map<int, Mesh>* geometry_lod13,
                                const std::unordered_map<int, Mesh>* geometry_lod22,
                                const AttributeMapRow attributes) {
    // Note: output_stream is ignored for FCB, we use the file path instead
    // Check if file path changed (split_cjseq mode - each building has its own file)
    std::string feature_file_path;
    if (!filepath_.empty()) {
      feature_file_path = filepath_;
    } else {
      throw rooferException("FCB writer: file path must be set in filepath_ before write_feature");
    }
    
    // Change extension from .city.jsonl to .fcb
    if (feature_file_path.size() >= 11 && feature_file_path.substr(feature_file_path.size() - 11) == ".city.jsonl") {
      feature_file_path = feature_file_path.substr(0, feature_file_path.length() - 11) + ".fcb";
    } else if (feature_file_path.size() >= 6 && feature_file_path.substr(feature_file_path.size() - 6) == ".jsonl") {
      feature_file_path = feature_file_path.substr(0, feature_file_path.length() - 6) + ".fcb";
    } else {
      feature_file_path += ".fcb";
    }
    
    // If file path changed (split mode), we need to close old writer and create new one
    if (current_file_path_ != feature_file_path && !current_file_path_.empty() && rust_writer_.has_value()) {
      // Close previous file before starting new one
      try {
        ::close_file(**rust_writer_);
      } catch (...) {
        // Ignore errors during close
      }
      rust_writer_.reset();
      metadata_written_ = false;
    }
    
    // If no writer or new file, create one with minimal metadata
    if (!rust_writer_.has_value() || !metadata_written_) {
      current_file_path_ = feature_file_path;
      
      // For split mode or first feature, create minimal metadata
      // We'll create a simple CityJSON structure with just the transform
      nlohmann::json metadataJSON;
      metadataJSON["type"] = "CityJSON";
      metadataJSON["version"] = "2.0";
      metadataJSON["CityObjects"] = nlohmann::json::object();
      metadataJSON["vertices"] = nlohmann::json::array();
      metadataJSON["transform"] = {{"scale", {scale_x_, scale_y_, scale_z_}},
                                  {"translate", {translate_x_, translate_y_, translate_z_}}};
      
      rust_writer_ = ::create_fcb_writer();
      std::string json_str = cityjson_to_string(metadataJSON);
      try {
        ::rust::Str json_str_rust(json_str.c_str());
        ::rust::Str file_path_rust(feature_file_path.c_str());
        // Split mode: one feature per file - use empty schema (single feature)
        ::rust::Str empty_schema_rust("{}");
        ::write_metadata(**rust_writer_, json_str_rust, file_path_rust, 1, empty_schema_rust);
      } catch (const std::exception& e) {
        throw rooferException("Failed to write FCB metadata: " + std::string(e.what()));
      }
      metadata_written_ = true;
    }

    if (!rust_writer_.has_value()) {
      throw rooferException("FCB writer: writer not initialized");
    }

    // Build CityJSONFeature structure (same as CityJsonWriter)
    nlohmann::json outputJSON;
    outputJSON["type"] = "CityJSONFeature";
    outputJSON["CityObjects"] = nlohmann::json::object();

    std::vector<arr3d> vertex_vec;
    write_cityobject_fcb(footprint, geometry_lod12, geometry_lod13, geometry_lod22, attributes,
                        outputJSON, vertex_vec, identifier_attribute,
                        std::to_string(++written_features_count), pjHelper);

    // Find the building ID
    for (auto& el : outputJSON["CityObjects"].items()) {
      if (!el.value().contains(std::string("parents"))) {
        outputJSON["id"] = el.key();
      }
    }

    // Convert vertices to integer coordinates
    std::vector<std::array<int, 3>> vertices_int;
    for (auto& vertex : vertex_vec) {
      vertices_int.push_back({int((vertex[0] - translate_x_) / scale_x_),
                             int((vertex[1] - translate_y_) / scale_y_),
                             int((vertex[2] - translate_z_) / scale_z_)});
    }
    outputJSON["vertices"] = vertices_int;

    // Serialize to JSON string and pass to Rust
    std::string json_str = cityjson_to_string(outputJSON);
    try {
      ::rust::Str json_str_rust(json_str.c_str());
      ::write_feature(**rust_writer_, json_str_rust);
    } catch (const std::exception& e) {
      throw rooferException("Failed to write FCB feature: " + std::string(e.what()));
    }
    
    // In split mode, each building has its own file, so we close after writing the feature
    // We detect this by checking if the file path will change on the next call
    // For now, we'll close when the path changes (handled above) or in destructor
  }

  void FcbWriter::close_current_file() {
    if (rust_writer_.has_value()) {
      try {
        ::close_file(**rust_writer_);
        rust_writer_.reset();
        metadata_written_ = false;
        current_file_path_.clear();
      } catch (const std::exception& e) {
        // Log error - this is critical for index writing
        throw rooferException("Failed to close FCB file (index may be missing): " + std::string(e.what()));
      }
    }
  }

  std::unique_ptr<CityJsonWriterInterface> createFcbWriter(
      roofer::misc::projHelperInterface& pjh) {
    return std::make_unique<FcbWriter>(pjh);
  }
}  // namespace roofer::io
