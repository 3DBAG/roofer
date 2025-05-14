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

#include <ogrsf_frmts.h>
#include <roofer/logger/logger.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <roofer/io/VectorReader.hpp>
#include <sstream>
#include <unordered_map>
#include <variant>

namespace roofer::io {

  namespace fs = std::filesystem;

  class VectorReaderOGR : public VectorReaderInterface {
    GDALDatasetUniquePtr poDS;
    OGRLayer* poLayer;

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
        if (error) {
          throw(rooferException(
              "[VectorReaderOGR] Could not get the extent of the layer"));
        }
        layer_extent = {extent.MinX, extent.MinY, 0,
                        extent.MaxX, extent.MaxY, 0};
      }
    }

    size_t get_feature_count() override {
      if (poLayer == nullptr) {
        throw(rooferException("[VectorReaderOGR] Layer is not open"));
      }
      return poLayer->GetFeatureCount();
    }

    void get_crs(SpatialReferenceSystemInterface* srs) override {
      if (poLayer == nullptr) {
        throw(rooferException("[VectorReaderOGR] Layer is not open"));
      }
      if (OGRSpatialReference* layerSRS = poLayer->GetSpatialRef()) {
        if (!srs->is_valid()) {
          char* pszWKT = NULL;
          layerSRS->exportToWkt(&pszWKT);
          srs->import_wkt(pszWKT);
          CPLFree(pszWKT);
        }
      }
    }

    void read_polygon(OGRPolygon* poPolygon,
                      std::vector<LinearRing>& polygons) {
      LinearRing gf_polygon;
      // for(auto& poPoint : poPolygon->getExteriorRing()) {
      OGRPoint poPoint;
      auto ogr_ering = poPolygon->getExteriorRing();

      // ensure we output ccw exterior ring
      if (ogr_ering->isClockwise()) {
        ogr_ering->reversePoints();
      }
      for (size_t i = 0; i < ogr_ering->getNumPoints() - 1; ++i) {
        ogr_ering->getPoint(i, &poPoint);
        std::array<float, 3> p = pjHelper.coord_transform_fwd(
            poPoint.getX(), poPoint.getY(),
            base_elevation == 0 ? poPoint.getZ() : base_elevation);
        gf_polygon.push_back(p);
      }
      // also read the interior rings (holes)
      for (size_t i = 0; i < poPolygon->getNumInteriorRings(); ++i) {
        auto ogr_iring = poPolygon->getInteriorRing(i);
        // ensure we output cw interior ring
        if (!ogr_iring->isClockwise()) {
          ogr_iring->reversePoints();
        }
        vec3f gf_iring;
        for (size_t j = 0; j < ogr_iring->getNumPoints() - 1; ++j) {
          ogr_iring->getPoint(j, &poPoint);
          std::array<float, 3> p = pjHelper.coord_transform_fwd(
              poPoint.getX(), poPoint.getY(),
              base_elevation == 0 ? poPoint.getZ() : base_elevation);
          gf_iring.push_back(p);
        }
        gf_polygon.interior_rings().push_back(gf_iring);
      }
      polygons.push_back(gf_polygon);
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

      poLayer->ResetReading();
      if (this->region_of_interest.has_value()) {
        // logger.info("Setting spatial filter");
        auto& roi = *this->region_of_interest;
        poLayer->SetSpatialFilterRect(roi.pmin[0], roi.pmin[1], roi.pmax[0],
                                      roi.pmax[1]);
      }

      OGRFeature* poFeature;
      while ((poFeature = poLayer->GetNextFeature()) != NULL)
      // for (auto &poFeature : poLayer)
      {
        // read feature geometry
        OGRGeometry* poGeometry;
        poGeometry = poFeature->GetGeometryRef();

        // FIXME: we should check if te layer geometrytype matches
        // with this feature's geometry type. Messy because they
        // can be a bit different eg. wkbLineStringZM and
        // wkbLineString25D
        if (poGeometry == nullptr) {
          continue;
        }

        if (auto roi = this->region_of_interest) {
          OGRPoint poPoint;
          poGeometry->Centroid(&poPoint);
          arr3d p = {poPoint.getX(), poPoint.getY(), poPoint.getZ()};
          if (!roi->intersects(p)) {
            continue;
          }
        }

        if (wkbFlatten(poGeometry->getGeometryType()) == wkbPolygon) {
          OGRPolygon* poPolygon = poGeometry->toPolygon();

          read_polygon(poPolygon, polygons);

          // area.push_back(float(poPolygon->get_Area()));
          // is_valid.push_back(bool(poPolygon->IsValid()));
          if (attributes)
            push_attributes(*poFeature, attributes, field_name_map);

        } else if (wkbFlatten(poGeometry->getGeometryType()) ==
                   wkbMultiPolygon) {
          OGRMultiPolygon* poMultiPolygon = poGeometry->toMultiPolygon();
          for (auto poly_it = poMultiPolygon->begin();
               poly_it != poMultiPolygon->end(); ++poly_it) {
            read_polygon(*poly_it, polygons);

            // area.push_back(float((*poly_it)->get_Area()));
            // is_valid.push_back(bool((*poly_it)->IsValid()));
            if (attributes)
              push_attributes(*poFeature, attributes, field_name_map);
          }
        } else {
          throw rooferException(
              "[VectorReaderOGR] Unsupported geometry type\n");
        }
      }
      if (polygons.size() > 0) {
        // std::cout << "pushed " << polygons.size() << " linear_ring
        // features...\n";
      }
    }
  };

  std::unique_ptr<VectorReaderInterface> createVectorReaderOGR(
      roofer::misc::projHelperInterface& pjh) {
    return std::make_unique<VectorReaderOGR>(pjh);
  };

}  // namespace roofer::io
