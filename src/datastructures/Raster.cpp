// This file is part of gfp-building-reconstruction
// Copyright (C) 2018-2022 Ravi Peters

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "Raster.hpp"
#include <limits>

namespace RasterTools {

  Raster::Raster(double cellsize, double min_x, double max_x, double min_y, double max_y):
      cellSize_(cellsize), minx_(min_x), maxx_(max_x), miny_(min_y), maxy_(max_y)
  {  
    dimx_ = (maxx_-minx_)/cellSize_ + 1;
    dimy_ = (maxy_-miny_)/cellSize_ + 1;
    vals_ = std::make_unique<std::vector<float>>();
    vals_->resize(dimx_*dimy_);
    // counts_ = std::make_unique<std::vector<int16_t>>();
    // counts_->resize(dimx_*dimy_);
  }
  Raster::Raster(const Raster& r)
  {
    cellSize_ = r.cellSize_;
    maxx_ = r.maxx_;
    minx_ = r.minx_;
    maxy_ = r.maxy_;
    miny_ = r.miny_;
    noDataVal_ = r.noDataVal_;
    dimx_ = (maxx_-minx_)/cellSize_ + 1;
    dimy_ = (maxy_-miny_)/cellSize_ + 1;
    vals_ = std::make_unique<std::vector<float>>(*r.vals_);
    // counts_ = std::make_unique<std::vector<int16_t>>(*r.counts_);
  }
  void Raster::operator=(const Raster& r)
  {
    cellSize_ = r.cellSize_;
    maxx_ = r.maxx_;
    minx_ = r.minx_;
    maxy_ = r.maxy_;
    miny_ = r.miny_;
    noDataVal_ = r.noDataVal_;
    dimx_ = (maxx_-minx_)/cellSize_ + 1;
    dimy_ = (maxy_-miny_)/cellSize_ + 1;
    vals_ = std::make_unique<std::vector<float>>(*r.vals_);
    // counts_ = std::make_unique<std::vector<int16_t>>(*r.counts_);
  }

  void Raster::prefill_arrays(alg a){
  if (a==MIN)
    noDataVal_ = std::numeric_limits<float>::max();
  else
    noDataVal_ = -std::numeric_limits<float>::max();
    
  std::fill(vals_->begin(), vals_->end(), noDataVal_);
  // std::fill(counts_->begin(), counts_->end(), 0);
  }

  bool Raster::add_point(double x, double y, double z, alg a)
  {
    bool first = (*vals_)[getLinearCoord(x,y)]==noDataVal_;
    if (a==MIN) {
      min(x,y,z);
    } else if (a==MAX) {
      max(x,y,z);
    }
    return first;
  }
  bool Raster::check_point(double x, double y)
  {
    auto col = getCol(x,y);
    if (col >= dimx_ || col < 0) return false;
    auto row = getRow(x,y);
    if (row >= dimy_ || row < 0) return false;
    
    return true;
  }

  // inline void Raster::avg(double &x, double &y, double &val)
  // {
  //   size_t c = getLinearCoord(x,y);
  //   (*vals_)[c]= ((*vals_)[c]*(*counts_)[c]+val)/((*counts_)[c]+1);
  //   ++(*counts_)[c];
  // }

  inline void Raster::min(double &x, double &y, double &val)
  {
    size_t c = getLinearCoord(x,y);
    if ((*vals_)[c]>val) (*vals_)[c] = val;
  }

  inline void Raster::max(double &x, double &y, double &val)
  {
    size_t c = getLinearCoord(x,y);
    if ((*vals_)[c]<val) (*vals_)[c] = val;
  }

  // inline void Raster::cnt(double &x, double &y)
  // {
  //   size_t c = getLinearCoord(x,y);
  //   ++(*counts_)[c];
  // }

  std::array<double,2> Raster::getColRowCoord(double x, double y) const
  {
    double r = (y-miny_) / cellSize_;
    double c = (x-minx_) / cellSize_;
    
    return {c,r};
  }

  size_t Raster::getRow(double x, double y) const
  {
    return static_cast<size_t>( floor((y-miny_) / cellSize_) );
  }
  size_t Raster::getCol(double x, double y) const
  {
    return static_cast<size_t>( floor((x-minx_) / cellSize_) );
  }

  size_t Raster::getLinearCoord(double x, double y) const
  {
    size_t r = static_cast<size_t>( floor((y-miny_) / cellSize_) );
    size_t c = static_cast<size_t>( floor((x-minx_) / cellSize_) );
    
    return r * dimx_ + c;
  }

  size_t Raster::getLinearCoord(size_t r, size_t c) const
  {    
    return r * dimx_ + c;
  }

  std::array<float,3> Raster::getPointFromRasterCoords(size_t col, size_t row) const
  {
    std::array<float,3> p;
    p[0] = minx_ + col*cellSize_ + cellSize_/2;
    p[1] = miny_ + row*cellSize_ + cellSize_/2;
    p[2] = (*vals_)[col+row*dimx_];
    return p;
  }

  double Raster::sample(double &x, double &y)
  {
    return (*vals_)[getLinearCoord(x,y)];
  }

  void Raster::set_val(size_t col, size_t row, double val) {
    (*vals_)[col+row*dimx_] = val;
  }
  
  double Raster::get_val(size_t col, size_t row) {
    return (*vals_)[col+row*dimx_];
  }

  bool Raster::isNoData(size_t col, size_t row) {
    return get_val(col, row) == noDataVal_;
  }
  bool Raster::isNoData(double &x, double &y) {
    return (*vals_)[getLinearCoord(x,y)] == noDataVal_;
  }

  void Raster::set_nodata(double new_nodata_val) {
    for (size_t i=0; i<dimx_*dimy_; ++i) {
      if ((*vals_)[i] == noDataVal_) {
        (*vals_)[i] = new_nodata_val;
      }
    }
    noDataVal_ = new_nodata_val;
  }

  void Raster::fill_nn(size_t window_size) {
    // set nodata to max float
    std::vector<float> new_vals(dimx_*dimy_);
    
    set_nodata(std::numeric_limits<float>::max());
    // iterate though raster pixels
    for(size_t col=0; col < dimx_; ++col) {
      for(size_t row=0; row < dimy_; ++row) {
        // if there is nodata here 
        if (get_val(col, row) == noDataVal_) {
          // look in window of size radius around this pixel and collect the minimal value
          size_t left = std::max(int(0), int(col)-int(window_size));
          size_t right = std::min(int(dimx_), int(col)+int(window_size));
          size_t bottom = std::max(int(0), int(row)-int(window_size));
          size_t top = std::min(int(dimy_), int(row)+int(window_size));
          double min_val = noDataVal_;
          for (size_t wc = left; wc < right; ++wc ) {
            for (size_t wr = bottom; wr < top; ++wr ) {
              min_val = std::min(min_val, get_val(wc, wr));
            }
          }
          new_vals[col+row*dimx_] = min_val;
        } else {
          new_vals[col+row*dimx_] = get_val(col, row);
        }
      }
    }
    (*vals_) = new_vals;
  }

  // void Raster::write(const char* WKGCS, alg a, void * dataPtr, const char* outFile)
  // {
  //   if( EQUALN(WKGCS, "EPSG:",5) ) {
  //     oSRS.importFromEPSG( atoi(WKGCS+5) );
  //   } else if (EQUALN(WKGCS, "EPSGA:",6)) {
  //     oSRS.importFromEPSGA( atoi(WKGCS+6) );
  //   }
  //   GDALAllRegister();
  //   GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
  //   GDALDataset *poDstDS;
  //   GDALDataType dataType;

  //   if (a == CNT)
  //     dataType = GDT_UInt16;
  //   else
  //     dataType = GDT_Float64;
    
  //   char **papszOptions = NULL;
  //   poDstDS = poDriver->Create( outFile, dimx_, dimy_, 1, dataType,
  //                papszOptions );
  //   double adfGeoTransform[6] = { minx_, cellSize_, 0, miny_, 0, cellSize_ };
  //   GDALRasterBand *poBand;
    
  //   poDstDS->SetGeoTransform( adfGeoTransform );
    
  //   //  std::cout << oSRS.SetWellKnownGeogCS( WKGCS );
  //   //  std::cout << pszSRS_WKT <<std::endl;
    
  //   char *pszSRS_WKT = NULL;
  //   oSRS.exportToWkt( &pszSRS_WKT );
  //   poDstDS->SetProjection( pszSRS_WKT );
  //   CPLFree( pszSRS_WKT );
    
  //   poBand = poDstDS->GetRasterBand(1);
  //   poBand->RasterIO( GF_Write, 0, 0, dimx_, dimy_,
  //           dataPtr, dimx_, dimy_, dataType, 0, 0 );
  //   poBand->SetNoDataValue(noDataVal);
  //   /* Once we're done, close properly the dataset */
  //   GDALClose( (GDALDatasetH) poDstDS );
  // }

}