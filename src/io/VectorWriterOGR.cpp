// This file is part of gfp-gdal
// Copyright (C) 2018-2022 Ravi Peters

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "VectorWriter.hpp"

#include <unordered_map>
#include <variant>
#include <fstream>
#include <iomanip>
#include "spdlog/spdlog.h"
#include <sstream>
#include <filesystem>

#include <ogrsf_frmts.h>

namespace fs = std::filesystem;

namespace roofer {

class VectorWriterOGR : public VectorWriterInterface {
  GDALDatasetUniquePtr poDS;

  inline void create_field(OGRLayer* layer, const std::string& name, OGRFieldType field_type) {
    OGRFieldDefn oField(name.c_str(), field_type);
    if (layer->CreateField(&oField) != OGRERR_NONE) {
      throw(rooferException("Creating field failed"));
    }
  }
  /// Find and replace a substring with another substring
  inline std::string find_and_replace(std::string str, std::string from, std::string to) {

    std::size_t start_pos = 0;

    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();
    }

    return str;
  }

  OGRPolygon create_polygon(const LinearRing& lr) {
    OGRPolygon ogrpoly;
    OGRLinearRing ogrring;
    // set exterior ring
    for (auto& g : lr) {
      auto coord_t = pjHelper.coord_transform_rev(g[0], g[1], g[2]);
      ogrring.addPoint(coord_t[0],
                      coord_t[1],
                      coord_t[2]);
    }
    ogrring.closeRings();
    ogrpoly.addRing(&ogrring);

    // set interior rings
    for (auto& iring : lr.interior_rings()) {
      OGRLinearRing ogr_iring;
      for (auto& g : iring) {
        auto coord_t = pjHelper.coord_transform_rev(g[0], g[1], g[2]);
        ogr_iring.addPoint(coord_t[0],
                          coord_t[1],
                          coord_t[2]);
      }
      ogr_iring.closeRings();
      ogrpoly.addRing(&ogr_iring);
    }
    return ogrpoly;
  }

  public:
  using VectorWriterInterface::VectorWriterInterface;

  void writePolygons(const std::string& source, 
      const std::vector<LinearRing>& polygons, 
      const AttributeVecMap& attributes,
      size_t begin,
      size_t end) override
  {
    std::string connstr = conn_string_;
    std::string gdaldriver = gdaldriver_;
    std::string layername = layername_;

    connstr = source;

    // auto& geom_term = vector_input("geometries");
    GDALDriver* driver;
    driver = GetGDALDriverManager()->GetDriverByName(gdaldriver.c_str());
    if (driver == nullptr) {
      throw(rooferException(gdaldriver + " driver not available"));
    }

    if(gdaldriver != "PostgreSQL"){
      auto fpath = fs::path(connstr);
      if(overwrite_file_) {
        if(fs::exists(fpath)) {
          try {
            fs::remove_all(fpath);
          } catch (const std::exception& e) {
            throw(rooferException(e.what()));
          }
        }
      }
      if (create_directories_) {
        fs::create_directories(fpath.parent_path());
          // spdlog::info("Creating directory {}", connstr);
      }
    }

    GDALDataset* dataSource = nullptr;
    dataSource = (GDALDataset*) GDALOpenEx(connstr.c_str(), GDAL_OF_VECTOR|GDAL_OF_UPDATE, NULL, NULL, NULL);
    if (dataSource == nullptr) {
      dataSource = driver->Create(connstr.c_str(), 0, 0, 0, GDT_Unknown, NULL);
    }
    if (dataSource == nullptr) {
      throw(rooferException("Starting database connection failed."));
    }
    if (do_transactions_) if (dataSource->StartTransaction() != OGRERR_NONE) {
      throw(rooferException("Starting database transaction failed.\n"));
    }

    // spdlog::info("Using driver {}", dataSource->GetDriverName());

    OGRwkbGeometryType wkbType;
    // if (geom_term.is_connected_type(typeid(LinearRing))) {
      wkbType = wkbPolygon;
    // } else if (geom_term.is_connected_type(typeid(LineString))) {
    //   wkbType = wkbLineString25D;
    // } else if (geom_term.is_connected_type(typeid(Mesh))) {
    //   wkbType = wkbMultiPolygon25D;
    // } else if (geom_term.is_connected_type(typeid(std::vector<TriangleCollection>)) || geom_term.is_connected_type(typeid(MultiTriangleCollection))) {
    //   // Note that in case of a MultiTriangleCollection we actually write the
    //   // TriangleCollections separately, and not the whole MultiTriangleCollection
    //   // to a single feature. That's why a MultiPolygon and not an aggregate of
    //   // multipolygons.
    //   wkbType = wkbMultiPolygon25D;
    // } else if (geom_term.is_connected_type(typeid(std::unordered_map<int, Mesh>))) {
    //   wkbType = wkbMultiPolygon25D;
    // }
    
    std::unordered_map<std::string, size_t> attr_id_map;
    size_t fcnt(0);
    
    OGRLayer* layer = nullptr;
    char** lco = nullptr;

    if (gdaldriver == "FileGDB") {
      lco = CSLSetNameValue(lco, "CREATE_MULTIPATCH", "YES");
    } 
    if (overwrite_layer_) { // FileGDB does not support OVERWRITE, and falls back to OpenFileGDB (no multipatch support) when appending
      lco = CSLSetNameValue(lco, "OVERWRITE", "YES");
    } else {
      lco = CSLSetNameValue(lco, "OVERWRITE", "NO");
      layer = dataSource->GetLayerByName(find_and_replace(layername, "-", "_").c_str());
    }

    // bool supports_list_attributes = gdaldriver != "ESRI Shapefile" && gdaldriver != "FileGDB";

    auto total_size = polygons.size();
    auto write_size = end - begin;
    // spdlog::info("creating {} geometry features", write_size);

    auto CRS = srs;
    if (layer == nullptr) {
      OGRSpatialReference oSRS;
      if(!srs.empty()) {
        oSRS.SetFromUserInput(CRS.c_str());
        layer = dataSource->CreateLayer(layername.c_str(), &oSRS, wkbType, lco);
        
        // We set normalise_for_visualisation to true, becuase it seems that GDAL expects as the first coordinate easting/longitude when constructing geometries
        pjHelper.set_rev_crs_transform(CRS.c_str(), true);
      } else {
        layer = dataSource->CreateLayer(layername.c_str(), nullptr, wkbType, lco);
      }
      // oSRS.SetAxisMappingStrategy(OAMS_AUTHORITY_COMPLIANT);
  

      // Create GDAL feature attributes
      for (auto& [name, vecvar] : attributes.get_attributes()) {
        // spdlog::info("Field {} has a size of {}", name, vec.size());

        if (auto vec = attributes.get_if<bool>(name)) {
          OGRFieldDefn oField(name.c_str(), OFTInteger);
          oField.SetSubType(OFSTBoolean);
          if (layer->CreateField(&oField) != OGRERR_NONE) {
            throw(rooferException("Creating field failed"));
          }
          // if (total_size != vec->size()) throw(rooferException("Number of attributes not equal to number of geometries [field name =" + name + "]"));
          attr_id_map[name] = fcnt++;
        } else if (auto vec = attributes.get_if<float>(name)) {
          create_field(layer, name, OFTReal);
          // if (total_size != vec->size()) throw(rooferException("Number of attributes not equal to number of geometries [field name =" + name + "]"));
          attr_id_map[name] = fcnt++;
        } else if (auto vec = attributes.get_if<int>(name)) {
          create_field(layer, name, OFTInteger64);
          // if (total_size != vec->size()) throw(rooferException("Number of attributes not equal to number of geometries [field name =" + name + "]"));
          attr_id_map[name] = fcnt++;
        } else if (auto vec = attributes.get_if<std::string>(name)) {
          create_field(layer, name, OFTString);
          // if (total_size != vec->size()) throw(rooferException("Number of attributes not equal to number of geometries [field name =" + name + "]"));
          attr_id_map[name] = fcnt++;
        } else if (auto vec = attributes.get_if<Date>(name)) {
          create_field(layer, name, OFTDate);
          // if (total_size != vec->size()) throw(rooferException("Number of attributes not equal to number of geometries [field name =" + name + "]"));
          attr_id_map[name] = fcnt++;
        } else if (auto vec = attributes.get_if<Time>(name)) {
          create_field(layer, name, OFTTime);
          // if (total_size != vec->size()) throw(rooferException("Number of attributes not equal to number of geometries [field name =" + name + "]"));
          attr_id_map[name] = fcnt++;
        } else if (auto vec = attributes.get_if<DateTime>(name)) {
          create_field(layer, name, OFTDateTime);
          // if (total_size != vec->size()) throw(rooferException("Number of attributes not equal to number of geometries [field name =" + name + "]"));
          attr_id_map[name] = fcnt++;
        }
      }
    } else {
      // Fields already exist, so we need to map the poly_input("attributes")
      // names to the gdal layer names
      // But: what if layer has a different set of attributes?
      fcnt = layer->GetLayerDefn()->GetFieldCount();
      for (auto& [name, vec] : attributes.get_attributes()) {
        // spdlog::info("Field {} has a size of {}", name, vec.size());
        // if (total_size != vec.size()) {
        //   throw(rooferException("Number of attributes not equal to number of geometries [field name =" + name + "]"));
        // }
        // attr_id_map[geoflow attribute name] = gdal field index
        for (int i=0; i < fcnt; i++) {
          auto fdef = layer->GetLayerDefn()->GetFieldDefn(i);
          if (strcmp(fdef->GetNameRef(), name.c_str()) == 0)
            attr_id_map[name] = i;
        }
      }
    }
    if (do_transactions_) if (dataSource->CommitTransaction() != OGRERR_NONE) {
      throw(rooferException("Creating database transaction failed.\n"));
    }
    if (do_transactions_) if (dataSource->StartTransaction() != OGRERR_NONE) {
      throw(rooferException("Starting database transaction failed.\n"));
    }

    for (size_t i = begin; i != end; ++i) {
      OGRFeature* poFeature;
      poFeature = OGRFeature::CreateFeature(layer->GetLayerDefn());
      // create a vec of features for the case where we write multiple feature rows (building with multiple parts)
      std::vector<OGRFeature*> poFeatures;
      // Add the attributes to the feature
      for (auto& [name, varvec] : attributes.get_attributes()) {
        // if (!term->get_data_vec()[i].has_value()) continue;

        if (auto vec = attributes.get_if<bool>(name)) {
          if ((*vec)[i].has_value()) {
            poFeature->SetField(attr_id_map[name], (*vec)[i].value());
          } else {
            poFeature->SetFieldNull(attr_id_map[name]);
          }
        } else if (auto vec = attributes.get_if<float>(name)) {
          if ((*vec)[i].has_value()) {
            poFeature->SetField(attr_id_map[name], (*vec)[i].value());
          } else {
            poFeature->SetFieldNull(attr_id_map[name]);
          }
        } else if (auto vec = attributes.get_if<int>(name)) {
          if ((*vec)[i].has_value()) {
            poFeature->SetField(attr_id_map[name], (*vec)[i].value());
          } else {
            poFeature->SetFieldNull(attr_id_map[name]);
          }
        } else if (auto vec = attributes.get_if<std::string>(name)) {
          if ((*vec)[i].has_value()) {
            poFeature->SetField(attr_id_map[name], (*vec)[i].value().c_str());
          } else {
            poFeature->SetFieldNull(attr_id_map[name]);
          }
        } else if (auto vec = attributes.get_if<Date>(name)) {
          if ((*vec)[i].has_value()) {
            poFeature->SetField(attr_id_map[name], (*vec)[i].value().year, (*vec)[i].value().month, (*vec)[i].value().day);
          } else {
            poFeature->SetFieldNull(attr_id_map[name]);
          }
        } else if (auto vec = attributes.get_if<Time>(name)) {
          if ((*vec)[i].has_value()) {
            poFeature->SetField(attr_id_map[name], 0, 0, 0, (*vec)[i].value().hour, (*vec)[i].value().minute, (*vec)[i].value().second, (*vec)[i].value().timeZone);
          } else {
            poFeature->SetFieldNull(attr_id_map[name]);
          }
        } else if (auto vec = attributes.get_if<DateTime>(name)) {
          if ((*vec)[i].has_value()) {
            poFeature->SetField(attr_id_map[name], (*vec)[i].value().date.year, (*vec)[i].value().date.month, (*vec)[i].value().date.day, (*vec)[i].value().time.hour, (*vec)[i].value().time.minute, (*vec)[i].value().time.second, (*vec)[i].value().time.timeZone);
          } else {
            poFeature->SetFieldNull(attr_id_map[name]);
          }
        }
      }

      // Geometry input type handling for the feature
      // Cast the incoming geometry to the appropriate GDAL type. Note that this
      // need to be in line with what is set for wkbType above.
      if (!polygons[i].size()) {
        // set to an empty geometry
        poFeature->SetGeometry(OGRGeometryFactory::createGeometry(wkbType));
      } else {
        // if (geom_term.is_connected_type(typeid(LinearRing))) {
          OGRPolygon ogrpoly = create_polygon(polygons[i]);
          poFeature->SetGeometry(&ogrpoly);
          poFeatures.push_back(poFeature);
        // } else {
        //   std::cerr << "Unsupported type of input geometry " << geom_term.get_connected_type().name() << std::endl;
        // }
      }

      for (auto poFeat : poFeatures) {
        if (layer->CreateFeature(poFeat) != OGRERR_NONE) {
          throw(rooferException("Failed to create feature in "+gdaldriver));
        }
        OGRFeature::DestroyFeature(poFeat);
      }

      if (i % transaction_batch_size_ == 0) {
        if (do_transactions_) if (dataSource->CommitTransaction() != OGRERR_NONE) {
          throw(rooferException("Committing features to database failed.\n"));
        }
        if (do_transactions_) if (dataSource->StartTransaction() != OGRERR_NONE) {
          throw(rooferException("Starting database transaction failed.\n"));
        }
      }
    }

    if (do_transactions_) if (dataSource->CommitTransaction() != OGRERR_NONE) {
      throw(rooferException("Committing features to database failed.\n"));
    }

    GDALClose(dataSource);
  }
};

std::unique_ptr<VectorWriterInterface> createVectorWriterOGR(projHelperInterface& pjh) {
  return std::make_unique<VectorWriterOGR>(pjh);
};

} // namespace roofer


//  else if (geom_term.is_connected_type(typeid(LineString))) {
//           OGRLineString ogrlinestring;
//           const LineString &ls = geom_term.get<LineString>(i);
//           for (auto &g : ls) {
//             auto coord_t = pjHelper.coord_transform_rev(g[0], g[1], g[2]);
//             ogrlinestring.addPoint(coord_t[0],
//                                   coord_t[1],
//                                   coord_t[2]);
//           }
//           poFeature->SetGeometry(&ogrlinestring);
//           poFeatures.push_back(poFeature);
//         } else if (geom_term.is_connected_type(typeid(std::vector<TriangleCollection>))) {
//           OGRMultiPolygon ogrmultipoly = OGRMultiPolygon();
//           auto& tcs = geom_term.get<std::vector<TriangleCollection>>(i);

//           for (auto& tc : tcs) {  
//             auto poFeature_ = poFeature->Clone();
//             for (auto &triangle : tc) {
//               OGRPolygon ogrpoly = OGRPolygon();
//               OGRLinearRing ring = OGRLinearRing();
//               for (auto &vertex : triangle) {
//                 auto coord_t = pjHelper.coord_transform_rev(vertex[0], vertex[1], vertex[2]);
//                 ring.addPoint(coord_t[0],
//                               coord_t[1],
//                               coord_t[2]);
//               }
//               ring.closeRings();
//               ogrpoly.addRing(&ring);
//               if (ogrmultipoly.addGeometry(&ogrpoly) != OGRERR_NONE) {
//                 printf("couldn't add triangle to MultiSurfaceZ");
//               }
//             }
//             poFeature_->SetGeometry(&ogrmultipoly);
//             poFeatures.push_back(poFeature_);
//           }
//           OGRFeature::DestroyFeature(poFeature);
//         } else if (geom_term.is_connected_type(typeid(MultiTriangleCollection))) {
//           auto&           mtcs = geom_term.get<MultiTriangleCollection>(i);

//           for (size_t j=0; j<mtcs.tri_size(); j++) {
//             const auto& tc = mtcs.tri_at(j);
//             auto poFeature_ = poFeature->Clone();

//             // create an empty multipolygon for this TriangleCollection
//             OGRMultiPolygon ogrmultipoly = OGRMultiPolygon();
//             for (auto& triangle : tc) {
//               OGRPolygon    ogrpoly = OGRPolygon();
//               OGRLinearRing ring    = OGRLinearRing();
//               for (auto& vertex : triangle) {
//                 auto coord_t = pjHelper.coord_transform_rev(vertex[0], vertex[1], vertex[2]);
//                 ring.addPoint(coord_t[0],
//                               coord_t[1],
//                               coord_t[2]);
//               }
//               ring.closeRings();
//               ogrpoly.addRing(&ring);
//               if (ogrmultipoly.addGeometry(&ogrpoly) != OGRERR_NONE) {
//                 printf("couldn't add triangle to MultiPolygonZ");
//               }
//             }
//             poFeature_->SetGeometry(&ogrmultipoly);
//             if (mtcs.has_attributes()) {
//               for (const auto& attr_map : mtcs.attr_at(j)) {
//                 if (attr_map.second.empty()) poFeature_->SetFieldNull(attr_id_map[attr_map.first]);
//                 else {
//                   // Since the 'attribute_value' type is a 'variant' and therefore
//                   // the 'attr_map' AttributeMap is a vector of variants, the
//                   // SetField method does not recognize the data type stored
//                   // within the variant. So it doesn't write the values unless we
//                   // put the values into an array with an explicit type. I tried
//                   // passing attr_map.second.data() to SetField but doesn't work.
//                   attribute_value v = attr_map.second[0];
//                   if (std::holds_alternative<int>(v)) {
//                     std::vector<int> val(attr_map.second.size());
//                     for (size_t h=0; h<attr_map.second.size(); h++) {
//                       val[h] = std::get<int>(attr_map.second[h]);
//                     }
//                     poFeature_->SetField(attr_id_map[attr_map.first], attr_map.second.size(), val.data());
//                   }
//                   else if (std::holds_alternative<float>(v)) {
//                     std::vector<double> val(attr_map.second.size());
//                     for (size_t h=0; h<attr_map.second.size(); h++) {
//                       val[h] = (double) std::get<float>(attr_map.second[h]);
//                     }
//                     poFeature_->SetField(attr_id_map[attr_map.first], attr_map.second.size(), val.data());
//                   }
//                   else if (std::holds_alternative<std::string>(v)) {
//                     // FIXME: needs to align the character encoding with the encoding of the database, otherwise will throw an 'ERROR:  invalid byte sequence for encoding ...'
//   //                  const char* val[attr_map.second.size()];
//   //                  for (size_t h=0; h<attr_map.second.size(); h++) {
//   //                    val[h] = std::get<std::string>(attr_map.second[h]).c_str();
//   //                  }
//   //                  poFeature_->SetField(attr_id_map[attr_map.first], attr_map.second.size(), val);
//                   }
//                   else if (std::holds_alternative<bool>(v)) {
//                     std::vector<int> val(attr_map.second.size());
//                     for (size_t h=0; h<attr_map.second.size(); h++) {
//                       val[h] = std::get<bool>(attr_map.second[h]);
//                     }
//                     poFeature_->SetField(attr_id_map[attr_map.first], attr_map.second.size(), val.data());
//                   }
//                   else throw(rooferException("Unsupported attribute value type for: " + attr_map.first));
//                 }
//               }

//               auto bp_id = std::to_string(mtcs.building_part_ids_[j]);
//               poFeature_->SetField(attr_id_map["building_part_id"], bp_id.c_str());
//             }
//             poFeatures.push_back(poFeature_);
//           }
//           OGRFeature::DestroyFeature(poFeature);
//         } else if (geom_term.is_connected_type(typeid(Mesh))) {
//           auto&           mesh         = geom_term.get<Mesh>(i);
//           OGRMultiPolygon ogrmultipoly = OGRMultiPolygon();
//           for (auto& poly : mesh.get_polygons()) {
//             auto ogrpoly = create_polygon(poly);
//             if (ogrmultipoly.addGeometry(&ogrpoly) != OGRERR_NONE) {
//               printf("couldn't add polygon to MultiPolygon");
//             }
//           }
//           poFeature->SetGeometry(&ogrmultipoly);
//           poFeatures.push_back(poFeature);
//         } else if (geom_term.is_connected_type(typeid(std::unordered_map<int, Mesh>))) {
//           const auto& meshes = geom_term.get<std::unordered_map<int, Mesh>>(i);

//           for ( const auto& [mid, mesh] : geom_term.get<std::unordered_map<int, Mesh>>(i) ) {
//             auto poFeature_ = poFeature->Clone();

//             OGRMultiPolygon ogrmultipoly = OGRMultiPolygon();
//             for (auto& poly : mesh.get_polygons()) {
//               auto ogrpoly = create_polygon(poly);
//               if (ogrmultipoly.addGeometry(&ogrpoly) != OGRERR_NONE) {
//                 printf("couldn't add polygon to MultiPolygonZ");
//               }
//             }

            // if(supports_list_attributes) {
//               size_t label_size = mesh.get_labels().size();
//               std::vector<int> val(label_size);
//               val = mesh.get_labels();
//               poFeature_->SetField(attr_id_map["labels"], label_size, val.data());
//             }

//             auto bp_id = std::to_string(mid);
//             poFeature_->SetField(attr_id_map["building_part_id"], bp_id.c_str());

//             poFeature_->SetGeometry(&ogrmultipoly);
//             poFeatures.push_back(poFeature_);
//           }
//           OGRFeature::DestroyFeature(poFeature);
//         }