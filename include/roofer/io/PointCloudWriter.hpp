#pragma once
#include <memory>
#include <roofer/common/datastructures.hpp>
#include <roofer/misc/projHelper.hpp>

namespace roofer::io {
  struct LASWriterInterface {
    roofer::misc::projHelperInterface& pjHelper;

    LASWriterInterface(roofer::misc::projHelperInterface& pjh)
        : pjHelper(pjh){};
    virtual ~LASWriterInterface() = default;

    virtual void write_pointcloud(PointCollection& pointcloud, std::string path,
                                  std::string output_crs = "") = 0;
  };

  std::unique_ptr<LASWriterInterface> createLASWriter(
      roofer::misc::projHelperInterface& pjh);
}  // namespace roofer::io
