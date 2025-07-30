// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

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

#pragma once

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

// #include <gdal_priv.h>
// #include <cpl_string.h>
// #include <cpl_conv.h>
// #include <ogr_spatialref.h>
namespace roofer {
  namespace RasterTools {
    enum alg { MIN,
               MAX,
               ZERO };
    class Raster {
     public:
      typedef std::array<float, 3> point3d;
      typedef std::array<float, 2> point2d;
      Raster(double cellsize, double min_x, double max_x, double min_y,
             double max_y);
      Raster(const Raster &);
      void operator=(const Raster &r);
      Raster() {};
      ~Raster() {};
      /**
       * Prefills the raster array based on the specified method.
       * @param[in] a The method to be used for prefilling the raster arrays:
       * MIN, MAX or ZERO.
       */
      void prefill_arrays(alg a);
      bool add_point(double x, double y, double z, alg a);
      /**
       * Adds a value to the raster at the specified x and y coordinates.
       * @param[in] x x coordinate of the point
       * @param[in] y y coordinate of the point
       * @param[in] val value to be added
       * @return True if the value was added, false if the point is outside the
       * raster.
       */
      bool add_value(double x, double y, double val);
      /**
       * Checks if the point is within the raster.
       * @param[in] x x coordinate of the point
       * @param[in] y y coordinate of the point
       * @return True if the point is within the raster, false otherwise.
       */
      bool check_point(double x, double y);
      // void add_raster(double x, double y, double z, alg a); // this seems
      // unused. Should it be removed?

      /**
       * Gets the row number for the given x and y coordinates.
       * @param[in] x x coordinate of the point
       * @param[in] y y coordinate of the point
       * @return The row number.
       */
      size_t getRow(double x, double y) const;
      /**
       * Gets the col number for the given x and y coordinates.
       * @param[in] x x coordinate of the point
       * @param[in] y y coordinate of the point
       * @return The col number.
       */
      size_t getCol(double x, double y) const;
      size_t getLinearCoord(double x, double y) const;
      size_t getLinearCoord(size_t r, size_t c) const;
      std::array<double, 2> getColRowCoord(double x, double y) const;
      /**
       * Get the 3D point at the center of the input raster cell.
       *
       * @param[in] col the column of the cell
       * @param[in] row the row of the cell
       * @return the 3D point in the center of the cell
       */
      point3d getPointFromRasterCoords(size_t col, size_t row) const;
      double getNoDataVal() { return noDataVal_; };
      double sample(double &x, double &y);
      void set_val(size_t col, size_t row, double val);
      double get_val(size_t col, size_t row);
      bool isNoData(size_t col, size_t row);
      bool isNoData(double &x, double &y);
      void set_nodata(double new_nodata_val);
      void fill_nn(size_t window_size);
      // void write(const char* WKGCS, alg a, void * dataPtr, const char*
      // outFile);

      // rasterise a polygon and return a list with points - one in the center
      // of each pixel inside the polygon in the polygon first point is *not*
      // repeated as last T should be a vector of arr<float,2> or arr<float,3>
      template <typename T>
      std::vector<point3d> rasterise_polygon(T &polygon,
                                             std::array<double, 2> cr_min,
                                             std::array<double, 2> cr_max,
                                             bool returnNoData = true) const {
        // code adapted from http://alienryderflex.com/polygon_fill/
        int n_nodes, pixelX, pixelY, i, j, swap;
        int n_vertices = polygon.size();
        std::vector<point3d> result;

        // perhaps we can specialise these to the bounding box of the polygon
        int IMAGE_TOP = std::max(0, (int)std::floor(cr_min[1])),
            IMAGE_BOT = std::min((int)dimy_, (int)std::ceil(cr_max[1])),
            IMAGE_LEFT = std::max(0, (int)std::ceil(cr_min[0])),
            IMAGE_RIGHT = std::min((int)dimx_, (int)std::floor(cr_max[0]));

        // Loop through the rows of the image.
        for (pixelY = IMAGE_TOP; pixelY < IMAGE_BOT; pixelY++) {
          std::vector<int>
              intersect_x;  // vector to hold the x-coordinates where the
                            // scanline intersects the polygon

          // Build a list of nodes.
          n_nodes = 0;
          j = n_vertices - 1;
          for (i = 0; i < n_vertices; i++) {
            auto pi =
                getColRowCoord((double)polygon[i][0], (double)polygon[i][1]);
            auto pj =
                getColRowCoord((double)polygon[j][0], (double)polygon[j][1]);
            // std::cerr << pi[0] << " " << pi[1] << "\n";
            // std::cerr << pj[0] << " " << pj[1] << "\n";
            if ((pi[1] < (double)pixelY && pj[1] >= (double)pixelY) ||
                (pj[1] < (double)pixelY && pi[1] >= (double)pixelY)) {
              intersect_x.push_back(
                  (int)(pi[0] +
                        (pixelY - pi[1]) / (pj[1] - pi[1]) * (pj[0] - pi[0])));
              ++n_nodes;
            }
            j = i;
          }

          if (n_nodes > 1) {
            std::sort(intersect_x.begin(), intersect_x.begin() + n_nodes);
          }

          // Fill the pixels between node pairs.
          for (i = 0; i < n_nodes; i += 2) {
            if (intersect_x[i] >= IMAGE_RIGHT) break;
            if (intersect_x[i + 1] > IMAGE_LEFT) {
              if (intersect_x[i] < IMAGE_LEFT) intersect_x[i] = IMAGE_LEFT;
              if (intersect_x[i + 1] > IMAGE_RIGHT)
                intersect_x[i + 1] = IMAGE_RIGHT;
              for (pixelX = intersect_x[i]; pixelX <= intersect_x[i + 1];
                   pixelX++) {
                auto p = getPointFromRasterCoords(pixelX, pixelY);
                if (p[2] == noDataVal_) {
                  if (returnNoData) {
                    result.push_back(p);
                  }
                } else {
                  result.push_back(p);
                }
              }
            }
          }
        }

        return result;
      };
      template <typename T>
      std::vector<point3d> rasterise_polygon(T &polygon,
                                             bool returnNoData = true) const {
        return rasterise_polygon(polygon, {0, 0},
                                 {double(dimx_), double(dimy_)}, returnNoData);
      };
      // template<> std::vector<point3d> rasterise_polygon(std::vector<point2d>&
      // polygon) const; template<> std::vector<point3d>
      // rasterise_polygon(std::vector<point3d>& polygon) const;

      double cellSize_, minx_, miny_, maxx_, maxy_;
      size_t dimx_, dimy_;
      double noDataVal_;
      // std::unique_ptr<std::vector<int16_t>> counts_;
      std::unique_ptr<std::vector<float>> vals_;

     private:
      void avg(double &x, double &y, double &val);
      void min(double &x, double &y, double &val);
      void max(double &x, double &y, double &val);
      /**
       * Adds the input value to the value at the given coordinate of the
       * raster.
       * @param[in] val value to be added
       * @param[in] x x coordinate
       * @param[in] y y coordinate
       */
      void add(double &x, double &y, double &val);
      // void cnt(double &x, double &y);
      // OGRSpatialReference oSRS;
    };
  }  // namespace RasterTools
}  // namespace roofer
