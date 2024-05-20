
#include <memory>

#include <roofer/misc/projHelper.hpp>
#include <roofer/common/datastructures.hpp>

namespace roofer {
  struct VectorReaderInterface {

    projHelperInterface& pjHelper;

    VectorReaderInterface(projHelperInterface& pjh) : pjHelper(pjh) {};
    virtual ~VectorReaderInterface() = default;

    virtual void open(const std::string& source) = 0;

    virtual void readPolygons(std::vector<LinearRing>&, AttributeVecMap* attributes=nullptr) = 0;
  };

  std::unique_ptr<VectorReaderInterface> createVectorReaderOGR(projHelperInterface& pjh);
}