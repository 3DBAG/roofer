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

#include <ogrsf_frmts.h>
#include <roofer/logger/logger.h>

#include <filesystem>
#include <laswriter.hpp>
#include <roofer/io/PointCloudWriter.hpp>

namespace roofer::io {

  namespace fs = std::filesystem;

  struct LASWriter : public LASWriterInterface {
    using LASWriterInterface::LASWriterInterface;

    void write_point_cloud_collection(const PointCollection& point_cloud,
                                      std::string path) {
      LASwriteOpener laswriteopener;
      laswriteopener.set_file_name(path.c_str());

      auto& logger = logger::Logger::get_logger();

      LASheader lasheader;
      lasheader.x_scale_factor = 0.01;
      lasheader.y_scale_factor = 0.01;
      lasheader.z_scale_factor = 0.01;
      lasheader.x_offset = 0.0;
      lasheader.y_offset = 0.0;
      lasheader.z_offset = 0.0;

      // lasheader.version_major = 1;
      // lasheader.version_minor = 4;
      // lasheader.header_size = 375;
      lasheader.point_data_format = 0;
      lasheader.point_data_record_length = 20;

      // std::cout << crs_wkt << std::endl;
      // std::cout << crs_wkt.size() << std::endl;
      // std::cout << strlen(crs_wkt.c_str()) << std::endl;
      if (pjHelper.srs->is_valid()) {
        auto crs_wkt = pjHelper.srs->export_wkt();
        lasheader.set_geo_ogc_wkt(crs_wkt.size(), crs_wkt.c_str());
      }
      // lasheader.set_global_encoding_bit(LAS_TOOLS_GLOBAL_ENCODING_BIT_OGC_WKT_CRS);

      LASpoint laspoint;
      laspoint.init(&lasheader, lasheader.point_data_format,
                    lasheader.point_data_record_length, 0);

      LASwriter* laswriter = laswriteopener.open(&lasheader);
      if (laswriter == 0) {
        logger.error("ERROR: could not open laswriter");
        return;
      }

      // bool found_offset = manager.data_offset.has_value();

      auto classification =
          point_cloud.attributes.get_if<int>("classification");
      auto intensity = point_cloud.attributes.get_if<float>("intensity");
      auto colors = point_cloud.attributes.get_if<arr3f>("colors");

      // todo throw warnings
      if (classification) {
        if (classification->size() != point_cloud.size()) {
          classification = nullptr;
        }
      }
      if (intensity) {
        if (intensity->size() != point_cloud.size()) {
          intensity = nullptr;
        }
      }
      if (colors) {
        if (colors->size() != point_cloud.size()) {
          colors = nullptr;
        }
      }

      size_t i = 0;
      for (auto& p_ : point_cloud) {
        auto p = pjHelper.coord_transform_rev(p_);
        laspoint.set_x(p[0]);
        laspoint.set_y(p[1]);
        laspoint.set_z(p[2]);
        if (classification) {
          laspoint.set_classification((*classification)[i].value());
        }
        if (intensity) {
          laspoint.set_intensity((*intensity)[i].value());
        }
        if (colors) {
          laspoint.set_R((*colors)[i].value()[0] * 65535);
          laspoint.set_G((*colors)[i].value()[1] * 65535);
          laspoint.set_B((*colors)[i].value()[2] * 65535);
        }

        laswriter->write_point(&laspoint);
        laswriter->update_inventory(&laspoint);

        if ((++i) % 100000000 == 0) logger.info("Written {0} points...", i);
      }

      laswriter->update_header(&lasheader, TRUE);
      laswriter->close();
      delete laswriter;
    }

    void write_pointcloud(PointCollection& pointcloud,
                          std::string path) override {
      fs::create_directories(fs::path(path).parent_path());
      write_point_cloud_collection(pointcloud, path);
    }
  };

  std::unique_ptr<LASWriterInterface> createLASWriter(
      roofer::misc::projHelperInterface& pjh) {
    return std::make_unique<LASWriter>(pjh);
  };
}  // namespace roofer::io
