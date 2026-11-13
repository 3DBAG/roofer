// Copyright (c) 2018-2026 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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

#include <ogrsf_frmts.h>
#include <roofer/logger/logger.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <functional>
#include <iostream>
#include <roofer/io/VectorReader.hpp>
#include <sstream>
#include <unordered_map>
#include <variant>

namespace roofer::io {

  namespace fs = std::filesystem;

  class VectorReaderOGR : public VectorReaderInterface {
    GDALDatasetUniquePtr poDS;
    OGRLayer* poLayer = nullptr;

    int layer_count = 0;
    float base_elevation = 0;
    bool output_fid_ = false;

    void push_attributes(const OGRFeature& poFeature,
                         AttributeVecMap* attributes,
                         std::unordered_map<std::string, int>& field_name_map) {
      for (auto& [name, idx] : field_name_map) {
        if (auto attr = attributes->get_if<bool>(name)) {
          if (poFeature.IsFieldNull(idx)) {
            attr->push_back(std::nullopt);
          } else {
            attr->push_back(bool(poFeature.GetFieldAsInteger(name.c_str())));
          }
        } else if (auto attr = attributes->get_if<int>(name)) {
          if (poFeature.IsFieldNull(idx)) {
            attr->push_back(std::nullopt);
          } else {
            if (name == "OGR_FID") {
              attr->push_back(int(poFeature.GetFID()));
            } else {
              attr->push_back(int(poFeature.GetFieldAsInteger64(name.c_str())));
            }
          }
        } else if (auto attr = attributes->get_if<float>(name)) {
          if (poFeature.IsFieldNull(idx)) {
            attr->push_back(std::nullopt);
          } else {
            attr->push_back(float(poFeature.GetFieldAsDouble(name.c_str())));
          }
        } else if (auto attr = attributes->get_if<std::string>(name)) {
          if (poFeature.IsFieldNull(idx)) {
            attr->push_back(std::nullopt);
          } else {
            attr->push_back(
                (std::string)poFeature.GetFieldAsString(name.c_str()));
          }
        } else if (auto attr = attributes->get_if<Date>(name)) {
          if (poFeature.IsFieldNull(idx)) {
            attr->push_back(std::nullopt);
          } else {
            DateTime t;
            poFeature.GetFieldAsDateTime(field_name_map[name], &t.date.year,
                                         &t.date.month, &t.date.day, nullptr,
                                         nullptr, &t.time.second, nullptr);
            attr->push_back(t.date);
          }
        } else if (auto attr = attributes->get_if<Time>(name)) {
          if (poFeature.IsFieldNull(idx)) {
            attr->push_back(std::nullopt);
          } else {
            Time time;
            poFeature.GetFieldAsDateTime(field_name_map[name], nullptr, nullptr,
                                         nullptr, &time.hour, &time.minute,
                                         &time.second, &time.timeZone);
            attr->push_back(time);
          }
        } else if (auto attr = attributes->get_if<DateTime>(name)) {
          if (poFeature.IsFieldNull(idx)) {
            attr->push_back(std::nullopt);
          } else {
            DateTime t;
            poFeature.GetFieldAsDateTime(
                field_name_map[name], &t.date.year, &t.date.month, &t.date.day,
                &t.time.hour, &t.time.minute, &t.time.second, &t.time.timeZone);
            attr->push_back(t);
          }
        }
      }
    }

    std::string to_wkt(OGRGeometry* geometry) {
      char* wkt = nullptr;
      geometry->exportToWkt(&wkt);
      std::string result = wkt == nullptr ? "" : wkt;
      CPLFree(wkt);
      return result;
    }

    bool should_skip_polygon(
        OGRPolygon* poPolygon, const OGRFeature& poFeature,
        std::optional<size_t> multipolygon_index = std::nullopt) {
      if (!skip_invalid_polygons || poPolygon->IsValid()) return false;

      auto& logger = logger::Logger::get_logger();
      if (multipolygon_index.has_value()) {
        logger.warning(
            "[VectorReaderOGR] Skipping invalid polygon. FID: {}, "
            "multipolygon index: {}, WKT: {}",
            poFeature.GetFID(), *multipolygon_index, to_wkt(poPolygon));
      } else {
        logger.warning(
            "[VectorReaderOGR] Skipping invalid polygon. FID: {}, WKT: {}",
            poFeature.GetFID(), to_wkt(poPolygon));
      }
      return true;
    }

   public:
    using VectorReaderInterface::VectorReaderInterface;

    void open(const std::string& source) override {
      auto& logger = logger::Logger::get_logger();

      // Open Dataset
      if (GDALGetDriverCount() == 0) GDALAllRegister();
      poDS = GDALDatasetUniquePtr(
          GDALDataset::Open(source.c_str(), GDAL_OF_VECTOR));
      if (poDS == nullptr) {
        // get error msg from GDAL
        auto error_msg = CPLGetLastErrorMsg();
        throw(rooferException("[VectorReaderOGR] Open failed on " + source +
                              " with error: " + error_msg));
      }

      // Open Layer
      layer_count = poDS->GetLayerCount();
      // logger.info("Layer count: {}", layer_count);

      poLayer = poDS->GetLayerByName(layer_name.c_str());
      if (poLayer == nullptr) {
        if (strict_layer_name && !layer_name.empty())
          throw rooferException(
              "[VectorReaderOGR] Requested layer does not exist: " +
              layer_name);
        if (layer_id >= layer_count) {
          throw(
              rooferException("[VectorReaderOGR] Illegal layer ID! Layer ID "
                              "must be less than the layer count."));
        } else if (layer_id < 0) {
          throw(
              rooferException("[VectorReaderOGR] Illegal layer ID! Layer ID "
                              "cannot be negative."));
        }
        poLayer = poDS->GetLayer(layer_id);
        // throw(rooferException("Could not get the selected layer by name=" +
        // layer_name));
      }
      if (poLayer == nullptr)
        throw(rooferException(
            "[VectorReaderOGR] Could not get the selected layer "));

      if (!expected_crs.empty() && poLayer->GetSpatialRef()) {
        OGRSpatialReference expected;
        if (expected.SetFromUserInput(expected_crs.c_str()) != OGRERR_NONE)
          throw rooferException("[VectorReaderOGR] Invalid expected CRS");
        expected.StripVertical();
        auto actual = std::unique_ptr<OGRSpatialReference>(
            poLayer->GetSpatialRef()->Clone());
        actual->StripVertical();
        expected.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        actual->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (!expected.IsSame(actual.get()))
          throw rooferException(
              "[VectorReaderOGR] Source does not use the expected horizontal "
              "CRS");
      }

      if (attribute_filter.size()) {
        auto error_code = poLayer->SetAttributeFilter(attribute_filter.c_str());
        if (OGRERR_NONE != error_code) {
          throw(rooferException(
              "[VectorReaderOGR] Invalid attribute filter: OGRErr=" +
              std::to_string(error_code) + ", filter=" + attribute_filter));
        }
        // compute extent of filtered layer
        OGREnvelope extent;
        for (auto& feature : poLayer) {
          OGRGeometry* geom = feature->GetGeometryRef();
          if (geom) {
            OGREnvelope gextent;
            geom->getEnvelope(&gextent);
            extent.Merge(gextent);
          }
        }
        // Also check for GDAL errors
        const char* gdal_error = CPLGetLastErrorMsg();
        if (CPLGetLastErrorType() != CE_None && gdal_error &&
            strlen(gdal_error) > 0) {
          throw rooferException(
              "[VectorReaderOGR] error while reading layer. Check your "
              "--filter string. GDAL error: " +
              std::string(gdal_error));
        }
        layer_extent = {extent.MinX, extent.MinY, 0,
                        extent.MaxX, extent.MaxY, 0};
      } else {
        OGREnvelope extent;
        auto error = poLayer->GetExtent(&extent);
        if (error && poLayer->GetFeatureCount() != 0) {
          throw(rooferException(
              "[VectorReaderOGR] Could not get the extent of the layer"));
        }
        if (error) extent.MinX = extent.MaxX = extent.MinY = extent.MaxY = 0;
        layer_extent = {extent.MinX, extent.MinY, 0,
                        extent.MaxX, extent.MaxY, 0};
      }
    }

    size_t get_feature_count() override {
      if (poLayer == nullptr) {
        throw(rooferException("[VectorReaderOGR] Layer is not open"));
      }
      auto feature_count = poLayer->GetFeatureCount();
      // Also check for GDAL errors
      const char* gdal_error = CPLGetLastErrorMsg();
      if (CPLGetLastErrorType() != CE_None && gdal_error &&
          strlen(gdal_error) > 0) {
        throw rooferException(
            "[VectorReaderOGR] error while reading layer. Check your "
            "--filter string. GDAL error: " +
            std::string(gdal_error));
      }
      return feature_count;
    }

    void get_crs(SpatialReferenceSystemInterface* srs) override {
      if (poLayer == nullptr) {
        throw(rooferException("[VectorReaderOGR] Layer is not open"));
      }
      if (auto* layerSRS = poLayer->GetSpatialRef()) {
        if (!srs->is_valid()) {
          char* pszWKT = NULL;
          layerSRS->exportToWkt(&pszWKT);
          srs->import_wkt(pszWKT);
          CPLFree(pszWKT);
        }
      }
    }

   private:
    template <typename Convert>
    auto read_ring(OGRLinearRing* ring, bool exterior, Convert convert) {
      using Coordinate = decltype(convert(0.0, 0.0, 0.0));
      std::vector<Coordinate> points;
      if (!ring) return points;
      if (bool(ring->isClockwise()) == exterior) ring->reversePoints();
      const int count = ring->getNumPoints() - (ring->get_IsClosed() ? 1 : 0);
      for (int i = 0; i < count; ++i)
        points.push_back(convert(ring->getX(i), ring->getY(i), ring->getZ(i)));
      return points;
    }

    void read_polygon(OGRPolygon* polygon, std::vector<LinearRing>& polygons) {
      auto convert = [&](double x, double y, double z) {
        return pjHelper.coord_transform_fwd(
            x, y, base_elevation == 0 ? z : base_elevation);
      };
      LinearRing result;
      auto exterior = read_ring(polygon->getExteriorRing(), true, convert);
      result.insert(result.end(), exterior.begin(), exterior.end());
      for (int i = 0; i < polygon->getNumInteriorRings(); ++i)
        result.interior_rings().push_back(
            read_ring(polygon->getInteriorRing(i), false, convert));
      polygons.push_back(std::move(result));
    }

    void for_each_polygon(
        const std::function<void(const OGRFeature&, OGRPolygon*)>& visit) {
      if (!poLayer)
        throw rooferException("[VectorReaderOGR] Layer is not open");
      poLayer->ResetReading();
      if (region_of_interest) {
        const auto& roi = *region_of_interest;
        poLayer->SetSpatialFilterRect(roi.pmin[0], roi.pmin[1], roi.pmax[0],
                                      roi.pmax[1]);
      } else {
        poLayer->SetSpatialFilter(nullptr);
      }
      for (auto&& feature : *poLayer) {
        auto* geometry = feature->GetGeometryRef();
        if (!geometry || geometry->IsEmpty()) continue;
        if (region_of_interest) {
          OGRPoint centroid;
          geometry->Centroid(&centroid);
          if (!region_of_interest->intersects(
                  arr3d{centroid.getX(), centroid.getY(), centroid.getZ()}))
            continue;
        }
        auto add = [&](OGRPolygon* polygon,
                       std::optional<size_t> part = std::nullopt) {
          if (!polygon->IsEmpty() &&
              !should_skip_polygon(polygon, *feature, part))
            visit(*feature, polygon);
        };
        const auto type = wkbFlatten(geometry->getGeometryType());
        if (type == wkbPolygon)
          add(geometry->toPolygon());
        else if (type == wkbMultiPolygon) {
          size_t part = 0;
          for (auto* polygon : *geometry->toMultiPolygon())
            add(polygon, part++);
        } else if (skip_unsupported_geometries) {
          logger::Logger::get_logger().warning(
              "[VectorReaderOGR] Skipping unsupported geometry, FID: {}",
              feature->GetFID());
        } else
          throw rooferException("[VectorReaderOGR] Unsupported geometry type");
      }
    }

   public:
    void readPolygons(
        std::vector<PolygonFeature>& polygons,
        const std::vector<std::string>& text_fields,
        const std::vector<std::string>& numeric_fields,
        const std::vector<std::string>& attribute_fields) override {
      if (!poLayer)
        throw rooferException("[VectorReaderOGR] Layer is not open");
      auto* definition = poLayer->GetLayerDefn();
      auto fields = [&](const auto& names, bool numeric) {
        std::vector<int> indices;
        for (const auto& name : names) {
          int index = definition->GetFieldIndex(name.c_str());
          if (index < 0)
            throw rooferException("[VectorReaderOGR] Missing attribute: " +
                                  name);
          auto type = definition->GetFieldDefn(index)->GetType();
          if (numeric && type != OFTReal && type != OFTInteger &&
              type != OFTInteger64)
            throw rooferException(
                "[VectorReaderOGR] Attribute must be numeric: " + name);
          indices.push_back(index);
        }
        return indices;
      };
      auto text_indices = fields(text_fields, false),
           numeric_indices = fields(numeric_fields, true),
           attribute_indices = fields(attribute_fields, false);
      for_each_polygon([&](const OGRFeature& feature, OGRPolygon* polygon) {
        PolygonFeature result;
        result.fid = feature.GetFID();
        auto convert = [](double x, double y, double z) {
          return arr3d{x, y, z};
        };
        result.exterior = read_ring(polygon->getExteriorRing(), true, convert);
        for (int i = 0; i < polygon->getNumInteriorRings(); ++i)
          result.holes.push_back(
              read_ring(polygon->getInteriorRing(i), false, convert));
        for (size_t i = 0; i < text_fields.size(); ++i) {
          auto& value = result.text_attributes[text_fields[i]];
          if (feature.IsFieldSetAndNotNull(text_indices[i]))
            value = feature.GetFieldAsString(text_indices[i]);
        }
        for (size_t i = 0; i < numeric_fields.size(); ++i) {
          auto& value = result.numeric_attributes[numeric_fields[i]];
          if (feature.IsFieldSetAndNotNull(numeric_indices[i]))
            value = feature.GetFieldAsDouble(numeric_indices[i]);
        }
        for (size_t i = 0; i < attribute_fields.size(); ++i) {
          const auto& name = attribute_fields[i];
          const auto index = attribute_indices[i];
          if (!feature.IsFieldSetAndNotNull(index)) {
            result.attributes.set_null(name);
            continue;
          }
          const auto* field = definition->GetFieldDefn(index);
          switch (field->GetType()) {
            case OFTInteger:
              if (field->GetSubType() == OFSTBoolean)
                result.attributes.insert(
                    name, bool(feature.GetFieldAsInteger(index)));
              else
                result.attributes.insert(name,
                                         feature.GetFieldAsInteger(index));
              break;
            case OFTInteger64:
              result.attributes.insert(
                  name, std::int64_t(feature.GetFieldAsInteger64(index)));
              break;
            case OFTReal:
              result.attributes.insert(name, feature.GetFieldAsDouble(index));
              break;
            default:
              result.attributes.insert(
                  name, std::string(feature.GetFieldAsString(index)));
          }
        }
        polygons.push_back(std::move(result));
      });
    }

    void readPolygons(std::vector<LinearRing>& polygons,
                      AttributeVecMap* attributes) override {
      auto& logger = logger::Logger::get_logger();

      // logger.info("Layer '{}' total feature count: {}", poLayer->GetName(),
      //             poLayer->GetFeatureCount());
      auto geometry_type = poLayer->GetGeomType();
      auto geometry_type_name = OGRGeometryTypeToName(geometry_type);
      // logger.info("Layer geometry type: {}", geometry_type_name);

      auto layer_def = poLayer->GetLayerDefn();
      auto field_count = layer_def->GetFieldCount();

      // Set up vertex data (and buffer(s)) and attribute pointers
      // LineStringCollection line_strings;
      // LinearRingCollection linear_rings;
      // std::vector<LinearRing> polygons;
      // auto &linear_rings = vector_output("linear_rings");
      // auto &line_strings = vector_output("line_strings");

      // auto &is_valid = vector_output("is_valid");
      // auto &area = vector_output("area");

      std::unordered_map<std::string, int> field_name_map;
      if (attributes) {
        if (output_fid_) {
          attributes->insert_vec<int>("OGR_FID");
          field_name_map["OGR_FID"] = -1;
        }
        for (size_t i = 0; i < field_count; ++i) {
          auto field_def = layer_def->GetFieldDefn(i);
          auto t = field_def->GetType();
          auto field_name = (std::string)field_def->GetNameRef();
          field_name_map[field_name] = i;
          if ((t == OFTInteger) && (field_def->GetSubType() == OFSTBoolean)) {
            attributes->insert_vec<bool>(field_name);
          } else if (t == OFTInteger || t == OFTInteger64) {
            attributes->insert_vec<int>(field_name);
          } else if (t == OFTString) {
            attributes->insert_vec<std::string>(field_name);
          } else if (t == OFTReal) {
            attributes->insert_vec<float>(field_name);
          } else if (t == OFTDate) {
            attributes->insert_vec<Date>(field_name);
          } else if (t == OFTTime) {
            attributes->insert_vec<Time>(field_name);
          } else if (t == OFTDateTime) {
            attributes->insert_vec<DateTime>(field_name);
          }
        }
      }

      for_each_polygon([&](const OGRFeature& feature, OGRPolygon* polygon) {
        read_polygon(polygon, polygons);
        if (attributes) push_attributes(feature, attributes, field_name_map);
      });
    }
  };

  std::unique_ptr<VectorReaderInterface> createVectorReaderOGR(
      roofer::misc::projHelperInterface& pjh) {
    return std::make_unique<VectorReaderOGR>(pjh);
  };

}  // namespace roofer::io
