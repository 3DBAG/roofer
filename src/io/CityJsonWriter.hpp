
#include "../datastructures.hpp"
#include "../misc/projHelper.hpp"
#include <cstddef>
#include <memory>

namespace roofer::io {
  struct CityJsonWriterInterface {

    // parameter variables
    std::string CRS_ = "EPSG:7415";
    std::string filepath_;
    std::string identifier_attribute = "";

    bool prettyPrint_ = false;
    bool only_output_renamed_ = false;

    vec1s key_options;
    StrMap output_attribute_names;

    float translate_x_ = 0.;
    float translate_y_ = 0.;
    float translate_z_ = 0.;
    float scale_x_ = 1.;
    float scale_y_ = 1.;
    float scale_z_ = 1.;

    projHelperInterface& pjHelper;

    CityJsonWriterInterface(projHelperInterface& pjh) : pjHelper(pjh) {};
    virtual ~CityJsonWriterInterface() = default;

    // add_poly_input("part_attributes", {typeid(bool), typeid(int), typeid(float), typeid(std::string), typeid(Date), typeid(Time), typeid(DateTime)});
    // add_poly_input("attributes", {typeid(bool), typeid(int), typeid(float), typeid(std::string), typeid(std::string), typeid(Date), typeid(Time), typeid(DateTime)});

    virtual void write(
      const std::string& source, 
      const std::vector<LinearRing >& footprints,
      const std::vector<std::unordered_map<int, Mesh> >* geometry_lod12,
      const std::vector<std::unordered_map<int, Mesh> >* geometry_lod13,
      const std::vector<std::unordered_map<int, Mesh> >* geometry_lod22,
      const AttributeVecMap& attributes) = 0;
  };

  std::unique_ptr<CityJsonWriterInterface> createCityJsonWriter(projHelperInterface& pjh);
}