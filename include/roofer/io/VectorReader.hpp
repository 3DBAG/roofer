#pragma once
#include <memory>

#include <roofer/misc/projHelper.hpp>
#include <roofer/common/datastructures.hpp>

namespace roofer::io {
  struct VectorReaderInterface {

    roofer::misc::projHelperInterface& pjHelper;

    VectorReaderInterface(roofer::misc::projHelperInterface& pjh) : pjHelper(pjh) {};
    virtual ~VectorReaderInterface() = default;

    virtual void open(const std::string& source) = 0;

    virtual void readPolygons(std::vector<LinearRing>&, AttributeVecMap* attributes=nullptr) = 0;
  };

  std::unique_ptr<VectorReaderInterface> createVectorReaderOGR(roofer::misc::projHelperInterface& pjh);
}