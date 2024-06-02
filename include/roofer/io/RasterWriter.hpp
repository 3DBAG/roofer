#pragma once
#include <cstddef>
#include <memory>
#include <roofer/common/datastructures.hpp>
#include <roofer/misc/projHelper.hpp>

namespace roofer::io {
  struct RasterWriterInterface {
    roofer::misc::projHelperInterface& pjHelper;

    RasterWriterInterface(roofer::misc::projHelperInterface& pjh)
        : pjHelper(pjh){};
    virtual ~RasterWriterInterface() = default;

    virtual void writeBands(const std::string& source, ImageMap& bands) = 0;
  };

  std::unique_ptr<RasterWriterInterface> createRasterWriterGDAL(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
