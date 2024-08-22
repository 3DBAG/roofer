// #include <proj.h>

#include <iostream>
#include <roofer/misc/projHelper.hpp>

namespace roofer::misc {

  struct projHelper : public projHelperInterface {
    using projHelperInterface::projHelperInterface;
    ReferenceSystem project_crs;

    void proj_clear() override { data_offset.reset(); };

    arr3f coord_transform_fwd(const double& x, const double& y,
                              const double& z) override {
      if (!data_offset.has_value()) {
        data_offset = {x, y, z};
      }
      auto result =
          arr3f{float(x - (*data_offset)[0]), float(y - (*data_offset)[1]),
                float(z - (*data_offset)[2])};

      return result;
    };
    arr3d coord_transform_rev(const float& x, const float& y,
                              const float& z) override {
      if (data_offset.has_value()) {
        return arr3d{x + (*data_offset)[0], y + (*data_offset)[1],
                     z + (*data_offset)[2]};
      } else {
        return arr3d{x, y, z};
      }
    }
    arr3d coord_transform_rev(const arr3f& p) override {
      return coord_transform_rev(p[0], p[1], p[2]);
    };
    const ReferenceSystem& crs() override { return project_crs; };
    void set_crs(const ReferenceSystem& crs) override { project_crs = crs; };

    void set_data_offset(arr3d& offset) override { data_offset = offset; }
  };

  std::unique_ptr<projHelperInterface> createProjHelper() {
    return std::make_unique<projHelper>();
  };
};  // namespace roofer::misc
// std::unique_ptr<projHelperInterface> createProjHelper(projHelperInterface& );
