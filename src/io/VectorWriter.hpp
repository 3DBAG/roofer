
#include "../datastructures.hpp"
#include "../misc/projHelper.hpp"
#include <cstddef>
#include <memory>

namespace roofer {
  struct VectorWriterInterface {

    std::string srs = "";// "EPSG:7415";
    std::string conn_string_ = "out";
    std::string gdaldriver_ = "GPKG";
    std::string layername_ = "geom";
    bool overwrite_layer_ = true;
    bool overwrite_file_ = true;
    bool create_directories_ = true;
    bool do_transactions_ = false;
    int transaction_batch_size_ = 1000;

    projHelperInterface& pjHelper;

    VectorWriterInterface(projHelperInterface& pjh) : pjHelper(pjh) {};
    virtual ~VectorWriterInterface() = default;

    virtual void writePolygons(
      const std::string& source, 
      const std::vector<LinearRing>& polygons, 
      const AttributeVecMap& attributes,
      size_t begin,
      size_t end) = 0;

    void writePolygons(
      const std::string& source, 
      const std::vector<LinearRing>& polygons, 
      const AttributeVecMap& attributes) {
      writePolygons(source, polygons, attributes, 0, polygons.size());
    };
  };

  std::unique_ptr<VectorWriterInterface> createVectorWriterOGR(projHelperInterface& pjh);
}