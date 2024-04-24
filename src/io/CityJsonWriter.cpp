#include "CityJsonWriter.hpp"
#include <nlohmann/json.hpp>
#include <set>
#include <fstream>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

namespace roofer::io {

  class CityJsonWriter : public CityJsonWriterInterface {
    template<typename T> void add_vertices_ring(std::map<arr3d, size_t>& vertex_map, std::vector<arr3d>& vertex_vec, std::set<arr3d>& vertex_set, const T& ring, Box& bbox) {
      size_t v_cntr = vertex_vec.size();
      for (auto &vertex_ : ring)
      {
        auto vertex = pjHelper.coord_transform_rev(vertex_);
        bbox.add(vertex);
        auto [it, did_insert] = vertex_set.insert(vertex);
        if (did_insert)
        {
          vertex_map[vertex] = v_cntr++;
          vertex_vec.push_back(vertex);
        }
      }
    }

    Box add_vertices_polygon(std::map<arr3d, size_t>& vertex_map, std::vector<arr3d>& vertex_vec, std::set<arr3d>& vertex_set, const LinearRing& polygon) {
      Box bbox;
      add_vertices_ring(vertex_map, vertex_vec, vertex_set, polygon, bbox);
      for (auto &iring : polygon.interior_rings()) {
        add_vertices_ring(vertex_map, vertex_vec, vertex_set, iring, bbox);
      }
      return bbox;
    }

    Box add_vertices_mesh(std::map<arr3d, size_t>& vertex_map, std::vector<arr3d>& vertex_vec, std::set<arr3d>& vertex_set, const Mesh& mesh) {
      Box bbox;
      for (auto &face : mesh.get_polygons())
      {
        bbox.add( add_vertices_polygon(vertex_map, vertex_vec, vertex_set, face) );
      }
      return bbox;
    }

    std::vector<std::vector<size_t>> LinearRing2jboundary(std::map<arr3d, size_t>& vertex_map, const LinearRing& face) {
      std::vector<std::vector<size_t>> jface;
      std::vector<size_t> exterior_ring;
      for (auto &vertex_ : face) {
        auto vertex = pjHelper.coord_transform_rev(vertex_);
        exterior_ring.push_back(vertex_map[vertex]);
      }
      jface.emplace_back(std::move(exterior_ring));
      for (auto &iring : face.interior_rings()) {
        std::vector<size_t> interior_ring;
        for (auto &vertex_ : iring) {
          auto vertex = pjHelper.coord_transform_rev(vertex_);
          interior_ring.push_back(vertex_map[vertex]);
        }
        jface.emplace_back(std::move(interior_ring));
      }
      return jface;
    }

    nlohmann::json::object_t mesh2jSolid(const Mesh& mesh, const char* lod, std::map<arr3d, size_t>& vertex_map) {
      auto geometry = nlohmann::json::object();
      geometry["type"] = "Solid";
      geometry["lod"] = lod;
      std::vector<std::vector<std::vector<size_t>>> exterior_shell;

      for (auto &face : mesh.get_polygons())
      {
        exterior_shell.emplace_back( LinearRing2jboundary(vertex_map, face) );
      }
      geometry["boundaries"] = {exterior_shell};

      auto surfaces = nlohmann::json::array();
      surfaces.push_back(nlohmann::json::object({{
        "type", "GroundSurface"
      }}));
      surfaces.push_back(nlohmann::json::object({{
        "type", "RoofSurface"
      }}));
      surfaces.push_back(nlohmann::json::object({
        {
          "type", "WallSurface"
        },
        {
          "on_footprint_edge", true
        }
      }));
      surfaces.push_back(nlohmann::json::object({
        {
          "type", "WallSurface"
        },
        {
          "on_footprint_edge", false
        }
      }));
      geometry["semantics"] = {
        {"surfaces", surfaces},
        {"values", {mesh.get_labels()}}
      };
      return geometry;
    }

    void write_to_file(const nlohmann::json& outputJSON, fs::path& fname, bool prettyPrint_)
    {
      fs::create_directories(fname.parent_path());
      std::ofstream ofs;
      ofs.open(fname);
      ofs << std::fixed << std::setprecision(2);
      try {
        if (prettyPrint_)
          ofs << outputJSON.dump(2);
        else
          ofs << outputJSON;
      } catch (const std::exception& e) {
        throw(rooferException(e.what()));
      }
    }

    // Computes the geographicalExtent array from a geoflow::Box and the data_offset from the NodeManager
    nlohmann::json::array_t compute_geographical_extent(Box& bbox) {
      auto minp = bbox.min();
      auto maxp = bbox.max();
      return {
        minp[0],
        minp[1],
        minp[2],
        maxp[0],
        maxp[1],
        maxp[2]
      };
    }

    void write_cityobjects(
      const std::vector<LinearRing >& footprints,
      const std::vector<std::unordered_map<int, Mesh> >* multisolids_lod12,
      const std::vector<std::unordered_map<int, Mesh> >* multisolids_lod13,
      const std::vector<std::unordered_map<int, Mesh> >* multisolids_lod22,
      const AttributeVecMap& attributes,
      nlohmann::json&                         outputJSON,
      std::vector<arr3d>&           vertex_vec,
      std::string&                  identifier_attribute,
      StrMap&                       output_attribute_names,
      bool&                         only_output_renamed)
    {
      std::map<arr3d, size_t> vertex_map;
      std::set<arr3d> vertex_set;
      size_t id_cntr = 0;
      size_t bp_counter = 0;

      // we expect at least one of the geomtry inputs is set
      bool export_lod12 = multisolids_lod12;
      bool export_lod13 = multisolids_lod13;
      bool export_lod22 = multisolids_lod22;
      size_t geometry_count = 0;
      if (export_lod12)
        geometry_count = multisolids_lod12->size();
      else if (export_lod13)
        geometry_count = multisolids_lod13->size();
      else if (export_lod22)
        geometry_count = multisolids_lod22->size();

      typedef std::unordered_map<int, Mesh> MeshMap;
      nlohmann::json j_null;
      for (size_t i=0; i<geometry_count; ++i) {
        auto building = nlohmann::json::object();
        auto b_id = std::to_string(++id_cntr);
        building["type"] = "Building";

        // Building atributes
        bool id_from_attr = false;
        auto jattributes = nlohmann::json::object();
        for (auto& [name_, vecvar] : attributes.get_attributes()) {
          std::string name = name_;
          //see if we need to rename this attribute
          auto search = output_attribute_names.find(name);
          if(search != output_attribute_names.end()) {
            //ignore if the new name is an empty string
            if(search->second.size()!=0)
              name = search->second;
          } else if (only_output_renamed) {
            continue;    
          }

          if (auto vec = attributes.get_if<bool>(name)) {
            if ((*vec)[i].has_value()) {
              jattributes[name] = (*vec)[i].value();
            } else {
              jattributes[name] = j_null;
            }
          } else if (auto vec = attributes.get_if<float>(name)) {
            if ((*vec)[i].has_value()) {
              jattributes[name] = (*vec)[i].value();
              if (name == identifier_attribute) {
                b_id = std::to_string((*vec)[i].value());
              }
            } else {
              jattributes[name] = j_null;
            }
          } else if (auto vec = attributes.get_if<int>(name)) {
            if ((*vec)[i].has_value()) {
              jattributes[name] = (*vec)[i].value();
              if (name == identifier_attribute) {
                b_id = std::to_string((*vec)[i].value());
                id_from_attr = true;
              }
            } else {
              jattributes[name] = j_null;
            }
          } else if (auto vec = attributes.get_if<std::string>(name)) {
            if ((*vec)[i].has_value()) {
              jattributes[name] = (*vec)[i].value();
              if (name == identifier_attribute) {
                b_id = (*vec)[i].value();
                id_from_attr = true;
              }
            } else {
              jattributes[name] = j_null;
            }
          // for date/time we follow https://en.wikipedia.org/wiki/ISO_8601
          } else if (auto vec = attributes.get_if<Date>(name)) {
            if ((*vec)[i].has_value()) {
              auto t = (*vec)[i].value();
              std::string date = t.format_to_ietf();
              jattributes[name] = date;
            } else {
              jattributes[name] = j_null;
            }
          } else if (auto vec = attributes.get_if<Time>(name)) {
            if ((*vec)[i].has_value()) {
              auto t = (*vec)[i].value();
              std::string time = std::to_string(t.hour) + ":" + std::to_string(t.minute) + ":" + std::to_string(t.second) + "Z";
              jattributes[name] = time;
            } else {
              jattributes[name] = j_null;
            }
          } else if (auto vec = attributes.get_if<DateTime>(name)) {
            if ((*vec)[i].has_value()) {
              auto t = (*vec)[i].value();
              std::string datetime = t.format_to_ietf();
              jattributes[name] = datetime;
            } else {
              jattributes[name] = j_null;
            }
          }
        }

        building["attributes"] = jattributes;

        // footprint geometry
        auto fp_geometry = nlohmann::json::object();
        fp_geometry["lod"] = "0";
        fp_geometry["type"] = "MultiSurface";

        const LinearRing footprint = footprints[i];
        add_vertices_polygon(vertex_map, vertex_vec, vertex_set, footprint);
        fp_geometry["boundaries"] = {LinearRing2jboundary(vertex_map, footprint)};
        building["geometry"].push_back(fp_geometry);

        std::vector<std::string> buildingPartIds;
        

        bool has_solids = false;
        if (export_lod12) has_solids = multisolids_lod12->at(i).size();
        if (export_lod13) has_solids = multisolids_lod13->at(i).size();
        if (export_lod22) has_solids = multisolids_lod22->at(i).size();

        Box building_bbox;
        if (has_solids) {
          MeshMap meshmap;
          if (export_lod22) {
            meshmap = multisolids_lod22->at(i);
          } else if (export_lod13) {
            meshmap = multisolids_lod13->at(i);
          } else if (export_lod12) {
            meshmap = multisolids_lod12->at(i);
          }
          for ( const auto& [sid, solid_lodx] : meshmap ) {
            auto buildingPart = nlohmann::json::object();
            auto bp_id = b_id + "-" + std::to_string(sid);

            buildingPartIds.push_back(bp_id);
            buildingPart["type"] = "BuildingPart";
            buildingPart["parents"] = {b_id};

            // Use try-except here for some rare cases when the sid's between different lod's do not line up (eg for very fragmented buildings from poor dim pointcloud).
            if (export_lod12) {
              try {
                building_bbox = add_vertices_mesh(vertex_map, vertex_vec, vertex_set, multisolids_lod12->at(i).at(sid));
                buildingPart["geometry"].push_back(mesh2jSolid(multisolids_lod12->at(i).at(sid), "1.2", vertex_map));
              } catch (const std::exception& e) {
                // std::cout << "skipping lod 12 building part\n";
              }
            }
            if (export_lod13) {
              try {
                building_bbox = add_vertices_mesh(vertex_map, vertex_vec, vertex_set, multisolids_lod13->at(i).at(sid));
                buildingPart["geometry"].push_back(mesh2jSolid(multisolids_lod13->at(i).at(sid), "1.3", vertex_map));
              } catch (const std::exception& e) {
                // std::cout << "skipping lod 13 building part\n";
              }
            }
            if (export_lod22) {
              building_bbox = add_vertices_mesh(vertex_map, vertex_vec, vertex_set, multisolids_lod22->at(i).at(sid));
              buildingPart["geometry"].push_back(mesh2jSolid(multisolids_lod22->at(i).at(sid), "2.2", vertex_map));
            }

            //attributes
            // auto jattributes = nlohmann::json::object();
            // for (auto& term : part_attributes.sub_terminals()) {
            //   auto tname = term->get_full_name();
            //   if (!term->get_data_vec()[i].has_value()) {
            //     nlohmann::json j_null;
            //     jattributes[tname] = j_null;
            //     continue;
            //   }

            //   if (term->accepts_type(typeid(bool))) {
            //     jattributes[tname] = term->get<const bool&>(bp_counter);
            //   } else if (term->accepts_type(typeid(float))) {
            //     jattributes[tname] = term->get<const float&>(bp_counter);
            //   } else if (term->accepts_type(typeid(int))) {
            //     jattributes[tname] = term->get<const int&>(bp_counter);
            //   } else if (term->accepts_type(typeid(std::string))) {
            //     jattributes[tname] = term->get<const std::string&>(bp_counter);
            //   }
            // }
            // ++bp_counter;
            // buildingPart["attributes"] = jattributes;

            outputJSON["CityObjects"][bp_id] = buildingPart;
          }
        }

        building["children"] = buildingPartIds;
        building["geographicalExtent"] = compute_geographical_extent(building_bbox);

        outputJSON["CityObjects"][b_id] = building;
      }
    }
    public:
    using CityJsonWriterInterface::CityJsonWriterInterface;
    void write(
      const std::string& source, 
      const std::vector<LinearRing >& footprints,
      const std::vector<std::unordered_map<int, Mesh> >* multisolids_lod12,
      const std::vector<std::unordered_map<int, Mesh> >* multisolids_lod13,
      const std::vector<std::unordered_map<int, Mesh> >* multisolids_lod22,
      const AttributeVecMap& attributes
    ) override {

      pjHelper.set_rev_crs_transform(CRS_.c_str());

      nlohmann::json outputJSON;

      outputJSON["type"] = "CityJSONFeature";
      outputJSON["CityObjects"] = nlohmann::json::object();

      std::vector<arr3d> vertex_vec;
      write_cityobjects(footprints,
                        multisolids_lod12,
                        multisolids_lod13,
                        multisolids_lod22,
                        attributes,
                        outputJSON,
                        vertex_vec,
                        identifier_attribute,
                        output_attribute_names,
                        only_output_renamed_);

      // The main Building is the parent object.
      // Bit of a hack. Ideally we would know exactly which ID we set,
      // instead of just iterating. But it is assumed that in case of writing to
      // CityJSONFeature there is only one parent CityObject.
      for (auto& el : outputJSON["CityObjects"].items()) {
        if (!el.value().contains(std::string("parents"))) {
          outputJSON["id"] = el.key();
        }
      };

      std::vector<std::array<int,3>>vertices_int;
      double _offset_x = translate_x_;
      double _offset_y = translate_y_;
      double _offset_z = translate_z_;
      for (auto& vertex : vertex_vec) {
        vertices_int.push_back({
          int( (vertex[0] - _offset_x ) / scale_x_ ),
          int( (vertex[1] - _offset_y ) / scale_y_ ),
          int( (vertex[2] - _offset_z ) / scale_z_ )
        });
      }
      outputJSON["vertices"] = vertices_int;

      fs::path fname = fs::path(source);
      write_to_file(outputJSON, fname, prettyPrint_);
      pjHelper.clear_rev_crs_transform();
    }
  };

  std::unique_ptr<CityJsonWriterInterface> createCityJsonWriter(projHelperInterface& pjh) {
    return std::make_unique<CityJsonWriter>(pjh);
  };
}