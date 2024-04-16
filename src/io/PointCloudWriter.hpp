

#include <memory>

#include "../datastructures.hpp"
#include "../misc/projHelper.hpp"

namespace roofer {
  struct LASWriterInterface {
    projHelperInterface& pjHelper;

    LASWriterInterface(projHelperInterface& pjh) : pjHelper(pjh){};
    virtual ~LASWriterInterface() = default;

    virtual void write_pointcloud(PointCollection& pointcloud, std::string path,
                                  std::string output_crs = "") = 0;
  };

  std::unique_ptr<LASWriterInterface> createLASWriter(projHelperInterface& pjh);
}  // namespace roofer
namespace roofer {}