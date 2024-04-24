// This file is part of gfp-gdal
// Copyright (C) 2018-2022 Ravi Peters

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "RasterWriter.hpp"

#include <unordered_map>
#include <variant>
#include <fstream>
#include <iomanip>
#include "spdlog/spdlog.h"
#include <sstream>
#include <filesystem>

#include <gdal_priv.h>

namespace fs = std::filesystem;

namespace roofer {

class RasterWriterGDAL : public RasterWriterInterface {

  std::string gdaldriver_ = "GTiff";
  bool create_directories_ = true;

  public:
  using RasterWriterInterface::RasterWriterInterface;

  void writeBands(
      const std::string& source, 
      ImageMap& bands) override {

    if (GDALGetDriverCount() == 0)
      GDALAllRegister();

    auto file_path = source;

    if(gdaldriver_ != "PostGISRaster" && create_directories_) fs::create_directories(fs::path(file_path).parent_path());
      
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(gdaldriver_.c_str());
    GDALDataset *poDstDS;
    GDALDataType dataType;
    
    dataType = GDT_Float32;
    
    char **papszOptions = nullptr;
    // TODO: should check if input images have the same dimension and cellsize....
    auto& image = (*bands.begin()).second;
    poDstDS = poDriver->Create( file_path.c_str(), image.dim_x, image.dim_y, bands.size(), dataType,
                                papszOptions );
    double adfGeoTransform[6] = { image.min_x + (*pjHelper.data_offset)[0], image.cellsize, 0, image.min_y + (*pjHelper.data_offset)[1], 0, image.cellsize };
    
    auto no_data_val = image.nodataval;
    
    poDstDS->SetGeoTransform( adfGeoTransform );
    
    //    std::cout << oSRS.SetWellKnownGeogCS( WKGCS );
    //    std::cout << pszSRS_WKT <<std::endl;
    
    char *pszSRS_WKT = nullptr;
  //    oSRS.exportToWkt( &pszSRS_WKT );
  //    poDstDS->SetProjection( pszSRS_WKT );
    CPLFree( pszSRS_WKT );
    
    size_t nBand = 1;
    GDALRasterBand *poBand;
    for (auto& [name, image] : bands) {

      // use same nodata value for all bands
      if (no_data_val != image.nodataval) {
        std::replace(image.array.begin(), image.array.end(), image.nodataval, no_data_val);
      }
    
      poBand = poDstDS->GetRasterBand(nBand++);
      auto error = poBand->RasterIO( GF_Write, 0, 0, image.dim_x, image.dim_y,
                        image.array.data(), image.dim_x, image.dim_y, dataType, 0, 0 );
      if (error == CE_Failure) {
        throw(rooferException("Unable to write to raster"));
      }
      poBand->SetNoDataValue(no_data_val);
      poBand->SetDescription(name.c_str());
    }
    /* Once we're done, close properly the dataset */
    GDALClose( (GDALDatasetH) poDstDS );
  }
};

std::unique_ptr<RasterWriterInterface> createRasterWriterGDAL(projHelperInterface& pjh) {
  return std::make_unique<RasterWriterGDAL>(pjh);
};

} // namespace roofer