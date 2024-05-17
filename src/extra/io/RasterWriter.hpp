
#include <cstddef>
#include <memory>

#include "projHelper.hpp"
#include "datastructures.hpp"

namespace roofer {
  struct RasterWriterInterface {

    projHelperInterface& pjHelper;

    RasterWriterInterface(projHelperInterface& pjh) : pjHelper(pjh) {};
    virtual ~RasterWriterInterface() = default;

    virtual void writeBands(
      const std::string& source, 
      ImageMap& bands) = 0;
  };

  std::unique_ptr<RasterWriterInterface> createRasterWriterGDAL(projHelperInterface& pjh);
}