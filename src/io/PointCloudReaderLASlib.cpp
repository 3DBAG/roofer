#include "PointCloudReader.hpp"
#include <lasreader.hpp>
#include <laswriter.hpp>

#include <iomanip>
#include "spdlog/spdlog.h"

namespace roofer {

class PointCloudReaderLASlib : public PointCloudReaderInterface {

    void getOgcWkt(LASheader* lasheader, std::string& wkt) {
        for (int i = 0; i < (int)lasheader->number_of_variable_length_records; i++)
        {
            if (lasheader->vlrs[i].record_id == 2111) // OGC MATH TRANSFORM WKT
            {
                spdlog::debug("Found and ignored: OGC MATH TRANSFORM WKT");
            }
            else if (lasheader->vlrs[i].record_id == 2112) // OGC COORDINATE SYSTEM WKT
            {
                spdlog::debug("Found: OGC COORDINATE SYSTEM WKT");
                wkt = (char *)(lasheader->vlrs[i].data);
            }
            else if (lasheader->vlrs[i].record_id == 34735) // GeoKeyDirectoryTag
            {
                spdlog::debug("Found and ignored: GeoKeyDirectoryTag");
            }
        }

        for (int i = 0; i < (int)lasheader->number_of_extended_variable_length_records; i++)
        {
            if (strcmp(lasheader->evlrs[i].user_id, "LASF_Projection") == 0)
            {
            if (lasheader->evlrs[i].record_id == 2111) // OGC MATH TRANSFORM WKT
            {
                spdlog::debug("Found and ignored: OGC MATH TRANSFORM WKT");

            }
            else if (lasheader->evlrs[i].record_id == 2112) // OGC COORDINATE SYSTEM WKT
            {
                spdlog::debug("Found: OGC COORDINATE SYSTEM WKT");
                wkt = (char *)(lasheader->evlrs[i].data);
            }
            }
        }
        spdlog::debug(wkt);
    }

    LASreader* lasreader;

  public:
  using PointCloudReaderInterface::PointCloudReaderInterface;

    void open(const std::string& source) {
        LASreadOpener lasreadopener;
        lasreadopener.set_file_name(source.c_str());
        lasreader = lasreadopener.open();
        if (!lasreader)
            throw(rooferException("Open failed on " + source));
    }

    virtual void readPointCloud(PointCollection& points, 
        vec1i* classification,
        vec1i* order,
        vec1f* intensities,
        vec3f* colors) {

        spdlog::debug("Attemting to find OGC CRS WKT...");
        // std::string wkt = manager.substitute_globals(wkt_);
        std::string wkt = "";
        getOgcWkt(&lasreader->header, wkt);
        if (wkt.size()!=0){
            pjHelper.set_fwd_crs_transform(wkt.c_str());
        }

        size_t i=0;
        while (lasreader->read_point()) {
            ++i;
            // if (do_class_filter && lasreader->point.get_classification() != filter_class) {
            // continue;
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
                order->push_back(float(i)/1000);
            }
            if (intensities) {
                intensities->push_back(float(lasreader->point.get_intensity()));
            }
            if (colors) {
                colors->push_back({
                    float(lasreader->point.get_R())/65535,
                    float(lasreader->point.get_G())/65535,
                    float(lasreader->point.get_B())/65535
                });
            }
            points.push_back(
                pjHelper.coord_transform_fwd(
                    lasreader->point.get_x(), 
                    lasreader->point.get_y(), 
                    lasreader->point.get_z()
                )
            );
        }
        pjHelper.clear_fwd_crs_transform();
        lasreader->close();
        delete lasreader;  
    }
};

std::unique_ptr<PointCloudReaderInterface> createPointCloudReaderLASlib(projHelperInterface& pjh) {
  return std::make_unique<PointCloudReaderLASlib>(pjh);
};

}