#pragma once
#include <memory>
#include <optional>

#include <roofer/common/datastructures.hpp>
#include <roofer/misc/projHelper.hpp>

namespace roofer::io {
  struct VectorReaderInterface {
    roofer::misc::projHelperInterface& pjHelper;
    std::optional<std::array<double, 4>> region_of_interest;

    VectorReaderInterface(roofer::misc::projHelperInterface& pjh)
        : pjHelper(pjh){};
    virtual ~VectorReaderInterface() = default;

    virtual void open(const std::string& source) = 0;

    virtual void readPolygons(std::vector<LinearRing>&,
                              AttributeVecMap* attributes = nullptr) = 0;
  };

  std::unique_ptr<VectorReaderInterface> createVectorReaderOGR(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
