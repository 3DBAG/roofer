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

#include <roofer/logger/logger.h>

#include <array>
#include <iomanip>
#include <lasreader.hpp>
#include <laswriter.hpp>
#include <roofer/io/PointCloudReader.hpp>

namespace roofer::io {

  struct PointCloudReaderLASlib : public PointCloudReaderInterface {
    ~PointCloudReaderLASlib() { close(); };

    void getOgcWkt(LASheader* lasheader, std::string& wkt) {
      auto& logger = logger::Logger::get_logger();

      //logger.debug("LAS Version: {}.{}", lasheader->version_major,
      //             lasheader->version_minor);
      //logger.debug("Point Format: {}", lasheader->point_data_format);
      //logger.debug("Number of Points: {}", lasheader->number_of_point_records);
      //logger.debug("Generating Software: {}", lasheader->generating_software);
      //logger.debug("Number of VLRs: {}",
      //             lasheader->number_of_variable_length_records);
      //logger.debug("Number of EVLRs: {}",
      //             lasheader->number_of_extended_variable_length_records);
      for (int i = 0; i < (int)lasheader->number_of_variable_length_records;
           i++) {
        //logger.debug("Found VLR with Record ID: {}",
        //             lasheader->vlrs[i].record_id);
        if (lasheader->vlrs[i].record_id == 2111)  // OGC MATH TRANSFORM WKT
        {
          //logger.debug("Found and ignored: OGC MATH TRANSFORM WKT");
        } else if (lasheader->vlrs[i].record_id ==
                   2112)  // OGC COORDINATE SYSTEM WKT
        {
          //logger.debug("Found: OGC COORDINATE SYSTEM WKT");
          wkt = (char*)(lasheader->vlrs[i].data);
        } else if (lasheader->vlrs[i].record_id == 34735)  // GeoKeyDirectoryTag
        {
          logger.debug("Found and ignored: GeoKeyDirectoryTag");
        }
      }

      for (int i = 0;
           i < (int)lasheader->number_of_extended_variable_length_records;
           i++) {
        if (strcmp(lasheader->evlrs[i].user_id, "LASF_Projection") == 0) {
          if (lasheader->evlrs[i].record_id == 2111)  // OGC MATH TRANSFORM WKT
          {
            logger.debug("Found and ignored: OGC MATH TRANSFORM WKT");

          } else if (lasheader->evlrs[i].record_id ==
                     2112)  // OGC COORDINATE SYSTEM WKT
          {
            //logger.debug("Found: OGC COORDINATE SYSTEM WKT");
            wkt = (char*)(lasheader->evlrs[i].data);
          }
        }
      }
      //logger.debug("LAS Wkt: {}", wkt);
    }

    LASreader* lasreader;

   public:
    using PointCloudReaderInterface::PointCloudReaderInterface;

    void open(const std::string& source) override {
      if (!lasreader) close();
      LASreadOpener lasreadopener;
      lasreadopener.set_file_name(source.c_str());
      lasreader = lasreadopener.open();
      if (lasreader == nullptr) {
        throw(rooferException("Open failed on " + source));
      }
    }

    void get_crs(SpatialReferenceSystemInterface* srs) override {
      std::string wkt;
      getOgcWkt(&lasreader->header, wkt);
      srs->import_wkt(wkt);
    }

    void close() override {
      if (lasreader) {
        lasreader->close();
        delete lasreader;
        lasreader = nullptr;
      }
    }

    TBox<double> getExtent() override {
      return {lasreader->get_min_x(), lasreader->get_min_y(),
              lasreader->get_min_z(), lasreader->get_max_x(),
              lasreader->get_max_y(), lasreader->get_max_z()};
    }

    virtual void readPointCloud(PointCollection& points, vec1i* classification,
                                vec1i* order, vec1f* intensities,
                                vec3f* colors) override {
      auto& logger = logger::Logger::get_logger();

      // if (wkt.size() != 0) {
      //   pjHelper.set_fwd_crs_transform(wkt.c_str());
      // }

      size_t i = 0;
      while (lasreader->read_point()) {
        ++i;
        // if (do_class_filter && lasreader->point.get_classification() !=
        // filter_class) { continue;
        // }
        // if (i % 1000000 == 0) {
        //   std::cout << "Read " << i << " points...\n";
        // }
        // if (thin_nth != 0) {
        //     if (i % thin_nth != 0) {
        //         continue;
        //     }
        // }
        if (classification) {
          classification->push_back(lasreader->point.get_classification());
        }
        if (order) {
          order->push_back(float(i) / 1000);
        }
        if (intensities) {
          intensities->push_back(float(lasreader->point.get_intensity()));
        }
        if (colors) {
          colors->push_back({float(lasreader->point.get_R()) / 65535,
                             float(lasreader->point.get_G()) / 65535,
                             float(lasreader->point.get_B()) / 65535});
        }
        points.push_back(pjHelper.coord_transform_fwd(
            lasreader->point.get_x(), lasreader->point.get_y(),
            lasreader->point.get_z()));
      }
    }
  };

  std::unique_ptr<PointCloudReaderInterface> createPointCloudReaderLASlib(
      roofer::misc::projHelperInterface& pjh) {
    return std::make_unique<PointCloudReaderLASlib>(pjh);
  };

}  // namespace roofer::io
