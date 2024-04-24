// #include <lasreader.hpp>
#include "PointCloudWriter.hpp"
#include <laswriter.hpp>

#include <filesystem>

#include <ogrsf_frmts.h>
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

namespace roofer {
  struct LASWriter : public LASWriterInterface {
    using LASWriterInterface::LASWriterInterface;
    
    void write_point_cloud_collection(const PointCollection& point_cloud, std::string path) {
      LASwriteOpener laswriteopener;
      laswriteopener.set_file_name(path.c_str());

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
      
      auto crs_wkt = pjHelper.get_rev_crs_wkt();
      // std::cout << crs_wkt << std::endl;
      // std::cout << crs_wkt.size() << std::endl;
      // std::cout << strlen(crs_wkt.c_str()) << std::endl;
      if (!crs_wkt.empty())
        lasheader.set_geo_ogc_wkt(crs_wkt.size(), crs_wkt.c_str());
      // lasheader.set_global_encoding_bit(LAS_TOOLS_GLOBAL_ENCODING_BIT_OGC_WKT_CRS);

      LASpoint laspoint;
      laspoint.init(&lasheader, lasheader.point_data_format, lasheader.point_data_record_length, 0);

      LASwriter* laswriter = laswriteopener.open(&lasheader);
      if (laswriter == 0)
      {
        spdlog::error("ERROR: could not open laswriter");
        return;
      }

      // bool found_offset = manager.data_offset.has_value();

      auto classification = point_cloud.attributes.get_if<int>("classification");
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

      size_t i=0;
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
        
        if((++i)%100000000==0) spdlog::info("Written {0} points...", i);
      } 

      laswriter->update_header(&lasheader, TRUE);
      laswriter->close();
      delete laswriter;
    }

    void write_pointcloud(
      PointCollection& pointcloud, 
      std::string path,
      std::string output_crs
    ) override {
      if (!output_crs.empty())
        pjHelper.set_rev_crs_transform(output_crs.c_str(), true);

      fs::create_directories(fs::path(path).parent_path());
      write_point_cloud_collection(pointcloud, path);
    }
  };

  std::unique_ptr<LASWriterInterface> createLASWriter(projHelperInterface& pjh) {
    return std::make_unique<LASWriter>(pjh);
  };
}