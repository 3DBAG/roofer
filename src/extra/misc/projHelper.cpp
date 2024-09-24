// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of geoflow-roofer (https://github.com/3DBAG/geoflow-roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ravi Peters

#include <iostream>
#include <roofer/misc/projHelper.hpp>

namespace roofer::misc {

  struct projHelper : public projHelperInterface {
    using projHelperInterface::projHelperInterface;
    // roofer::io::SpatialReferenceSystemInterface project_crs;

    void clear() override { data_offset.reset(); };

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
    // const ReferenceSystem& crs() override { return project_crs; };
    // void set_crs(const ReferenceSystem& crs) override { project_crs = crs; };

    void set_data_offset(arr3d& offset) override { data_offset = offset; }
  };

  std::unique_ptr<projHelperInterface> createProjHelper() {
    return std::make_unique<projHelper>();
  };
};  // namespace roofer::misc
// std::unique_ptr<projHelperInterface> createProjHelper(projHelperInterface& );
