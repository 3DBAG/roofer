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
// Balazs Dukai
// Gina Stavropoulou

#include <algorithm>
#include <numeric>
#include <random>
#include <roofer/common/Raster.hpp>
#include <roofer/common/datastructures.hpp>
#include <roofer/common/GridPIPTester.hpp>
#include <roofer/misc/PointcloudRasteriser.hpp>
// #include <roofer/logger/logger.h>

namespace roofer::misc {

  void RasterisePointcloud(PointCollection& pointcloud, LinearRing& footprint,
                           ImageMap& image_bundle,
                           // RasterTools::Raster& heightfield,
                           float cellsize, int ground_class,
                           int building_class) {
    // TODO: this is always true?
    bool use_footprint = true;
    Box box;
    if (use_footprint) {
      box = footprint.box();
    } else {
      box = pointcloud.box();
    }
    auto boxmin = box.min();
    auto boxmax = box.max();

    RasterTools::Raster r_max(cellsize, boxmin[0], boxmax[0], boxmin[1],
                              boxmax[1]);
    r_max.prefill_arrays(RasterTools::MAX);

    RasterTools::Raster r_min(r_max), r_fp(r_max), r_grp(r_max),
        r_ground_points(r_max), r_non_ground_points(r_max);
    r_min.prefill_arrays(RasterTools::MIN);
    r_fp.prefill_arrays(RasterTools::MAX);
    r_grp.prefill_arrays(RasterTools::ZERO);
    r_ground_points.prefill_arrays(RasterTools::ZERO);
    r_non_ground_points.prefill_arrays(RasterTools::ZERO);

    std::vector<std::vector<float>> buckets(r_max.dimx_ * r_max.dimy_);

    if (use_footprint) {
      auto fp_grid = GridPIPTester(footprint);

      for (size_t col = 0; col < r_fp.dimx_; ++col) {
        for (size_t row = 0; row < r_fp.dimy_; ++row) {
          auto p = r_fp.getPointFromRasterCoords(col, row);
          if (fp_grid.test(p)) {
            r_fp.add_point(p[0], p[1], 1, RasterTools::MAX);
          } else {
            r_fp.add_point(p[0], p[1], 0, RasterTools::MAX);
          }
        }
      }
    }

    auto classification = pointcloud.attributes.get_if<int>("classification");
    for (size_t pi = 0; pi < pointcloud.size(); ++pi) {
      auto& p = pointcloud[pi];
      auto& c = (*classification)[pi];
      if (r_max.check_point(p[0], p[1])) {
        if (c == ground_class) {
          r_ground_points.add_value(p[0], p[1], 1);
        } else if (c == building_class) {
          r_non_ground_points.add_value(p[0], p[1], 1);
        }

        r_max.add_point(p[0], p[1], p[2], RasterTools::MAX);
        r_min.add_point(p[0], p[1], p[2], RasterTools::MIN);
        buckets[r_max.getLinearCoord(p[0], p[1])].push_back(p[2]);
      }
    }

    // PointCollection grid_points;
    // vec1f values;
    // double nodata = r_max.getNoDataVal();
    image_bundle["max"] = Image{};
    image_bundle["max"].dim_x = r_max.dimx_;
    image_bundle["max"].dim_y = r_max.dimy_;
    image_bundle["max"].min_x = r_max.minx_;
    image_bundle["max"].min_y = r_max.miny_;
    image_bundle["max"].cellsize = r_max.cellSize_;
    image_bundle["max"].nodataval = r_max.noDataVal_;
    image_bundle["max"].array = *r_max.vals_;

    image_bundle["min"] = image_bundle["max"];
    image_bundle["min"].nodataval = r_min.noDataVal_;
    image_bundle["min"].array = *r_min.vals_;
    image_bundle["fp"] = image_bundle["max"];
    image_bundle["fp"].array = *r_fp.vals_;
    image_bundle["cnt"] = image_bundle["max"],
    image_bundle["med"] = image_bundle["max"],
    image_bundle["avg"] = image_bundle["max"],
    image_bundle["var"] = image_bundle["max"],
    image_bundle["grp"] = image_bundle["max"];
    image_bundle["gp"] = image_bundle["max"];
    image_bundle["gp"].nodataval = r_ground_points.noDataVal_;
    image_bundle["gp"].array = *r_ground_points.vals_;
    image_bundle["ngp"] = image_bundle["max"];
    image_bundle["ngp"].nodataval = r_non_ground_points.noDataVal_;
    image_bundle["ngp"].array = *r_non_ground_points.vals_;

    for (size_t row = 0; row < r_max.dimy_; ++row) {
      for (size_t col = 0; col < r_max.dimx_; ++col) {
        // auto p = r_max.getPointFromRasterCoords(col,row);
        // if (p[2]!=nodata) {
        // grid_points.push_back(p);
        // values.push_back(p[2]);
        // }
        auto lc = r_max.getLinearCoord(row, col);
        auto& buck = buckets.at(lc);
        if (buck.size() == 0) {
          image_bundle["cnt"].array[lc] = image_bundle["cnt"].nodataval;
          image_bundle["med"].array[lc] = image_bundle["med"].nodataval;
          image_bundle["avg"].array[lc] = image_bundle["avg"].nodataval;
          image_bundle["var"].array[lc] = image_bundle["var"].nodataval;
          image_bundle["grp"].array[lc] = image_bundle["grp"].nodataval;
        } else {
          std::sort(buck.begin(), buck.end());
          image_bundle["cnt"].array[lc] = buck.size();
          image_bundle["med"].array[lc] = buck[buck.size() / 2];
          image_bundle["avg"].array[lc] =
              std::accumulate(buck.begin(), buck.end(), 0) / buck.size();
          image_bundle["grp"].array[lc] =
              abs(image_bundle["gp"].array[lc] -
                  image_bundle["ngp"].array[lc]) /
              (image_bundle["gp"].array[lc] + image_bundle["ngp"].array[lc]);
          int ssum = 0;
          for (auto& z : buck) {
            ssum += std::pow(z - image_bundle["avg"].array[lc], 2);
          }
          image_bundle["var"].array[lc] = ssum / buck.size();
        }
      }
    }
    image_bundle.erase("gp");
    image_bundle.erase("ngp");
  }

  size_t getLinearCoord(const Image& im, double x, double y) {
    size_t r = static_cast<size_t>(floor((y - im.min_y) / im.cellsize));
    size_t c = static_cast<size_t>(floor((x - im.min_x) / im.cellsize));

    return r * im.dim_x + c;
  }
  void gridthinPointcloud(PointCollection& pointcloud, const Image& cnt_image,
                          float max_density) {
    const auto max_cnt_per_cell =
        max_density * (cnt_image.cellsize * cnt_image.cellsize);

    // use a fixed seed to make the result deterministic over multiple runs
    // std::random_device rd;
    // std::mt19937 gen(rd);
    std::mt19937 gen(31415);
    std::uniform_real_distribution<> dis(0.0, 1.0);

    PointCollection thinned_points;
    auto& thinned_classification =
        thinned_points.attributes.insert_vec<int>("classification");
    auto classification = pointcloud.attributes.get_if<int>("classification");
    for (size_t pi = 0; pi < pointcloud.size(); ++pi) {
      auto& p = pointcloud[pi];
      auto& c = (*classification)[pi];
      auto i = getLinearCoord(cnt_image, p[0], p[1]);
      if (i < 0 || i > cnt_image.array.size()) continue;

      // check if we have too many points in this cell
      if (cnt_image.array[i] > max_cnt_per_cell) {
        // generate random number [0-1] and compare to the fraction of points we
        // need to preserve in this cell
        if (dis(gen) <= (max_cnt_per_cell / cnt_image.array[i])) {
          thinned_points.push_back(p);
          thinned_classification.push_back(c);
        }
      } else {
        thinned_points.push_back(p);
        thinned_classification.push_back(c);
      }
    }
    pointcloud = thinned_points;
  }

  float computePointDensity(const ImageMap& pc) {
    const auto& fp = pc.at("fp").array;
    const auto& cnt = pc.at("cnt").array;
    const auto& cnt_nodata = pc.at("cnt").nodataval;
    auto& cellsize = pc.at("fp").cellsize;
    size_t fp_cnt{0}, pt_cnt{0};
    for (size_t i = 0; i < fp.size(); ++i) {
      if (fp[i] != 0 && cnt[i] != cnt_nodata) {
        ++fp_cnt;
        pt_cnt += cnt[i];
      }
    }
    // spdlog::debug("pt_cnt: {}, fp_cnt: {}, cellsize: {}", pt_cnt, fp_cnt,
    // cellsize);
    if (fp_cnt == 0) {
      return 0;
    } else {
      auto cell_area = cellsize * cellsize;
      return float(pt_cnt) / float(fp_cnt * cell_area);
    }
  }

  float computeRoofElevation(const ImageMap& pc, float percentile) {
    const auto& fp = pc.at("fp").array;
    const auto& h_max = pc.at("max").array;
    const auto& nodata = pc.at("max").nodataval;
    // auto& cellsize = pc.at("fp").cellsize;
    std::vector<float> h_max_in_fp;
    h_max_in_fp.reserve(fp.size());
    for (size_t i = 0; i < fp.size(); ++i) {
      if (fp[i] != 0 && h_max[i] != nodata) {
        h_max_in_fp.push_back(h_max[i]);
      }
    }
    if (h_max_in_fp.size() == 0) {
      return 0;
    }
    // sort the values
    std::sort(h_max_in_fp.begin(), h_max_in_fp.end());
    // get the 70th percentile
    return h_max_in_fp[h_max_in_fp.size() * percentile];
  }

  bool testForGlassRoof(const ImageMap& pc, float threshold_glass_roof) {
    auto& grp = pc.at("grp").array;
    auto& cellsize = pc.at("grp").cellsize;
    auto& grp_nodata = pc.at("grp").nodataval;
    float grp_mean{0};
    int grp_cnt{0};
    std::vector<float> grp_vals;
    for (size_t i = 0; i < grp.size(); ++i) {
      if (grp[i] != grp_nodata) {
        ++grp_cnt;
        grp_mean += grp[i];
        grp_vals.push_back(grp[i]);
      }
    }
    grp_mean = grp_mean / grp_cnt;

    if (grp_mean < threshold_glass_roof) {
      return true;
    }
    return false;
  }

  float computeNoDataFraction(const ImageMap& pc) {
    auto& fp = pc.at("fp").array;
    auto& cnt = pc.at("cnt").array;
    auto& cnt_nodata = pc.at("cnt").nodataval;
    size_t fp_cnt{0}, data_cnt{0};
    for (size_t i = 0; i < fp.size(); ++i) {
      if (fp[i] != 0) {
        ++fp_cnt;
        if (cnt[i] != cnt_nodata) {
          ++data_cnt;
        }
      }
    }
    if (fp_cnt == 0) {
      return 0;
    } else {
      double data_frac = double(data_cnt) / double(fp_cnt);
      return float(1 - data_frac);
    }
  }

  std::vector<bool> computeMask(const std::vector<float>& image_array,
                                const float& nodataval) {
    std::vector<bool> mask;
    mask.reserve(image_array.size());
    for (const auto& cell : image_array) {
      if (cell == nodataval) {
        mask.push_back(false);
      } else {
        mask.push_back(true);
      }
    }
    return mask;
  }

  bool isMutated(const ImageMap& a, const ImageMap& b,
                 const float& threshold_mutation_fraction,
                 const float& threshold_mutation_difference) {
    auto footprint_mask = computeMask(a.at("fp").array, 0);
    auto data_mask_a = computeMask(a.at("max").array, a.at("max").nodataval);
    auto data_mask_b = computeMask(b.at("max").array, b.at("max").nodataval);

    std::vector<bool> all_mask;
    all_mask.resize(footprint_mask.size());
    std::transform(data_mask_a.begin(), data_mask_a.end(), data_mask_b.begin(),
                   all_mask.begin(), std::multiplies<>());
    std::transform(all_mask.begin(), all_mask.end(), footprint_mask.begin(),
                   all_mask.begin(), std::multiplies<>());

    std::vector<float> all_mask_on_a;
    all_mask_on_a.resize(all_mask.size());
    std::transform(a.at("max").array.begin(), a.at("max").array.end(),
                   all_mask.begin(), all_mask_on_a.begin(),
                   std::multiplies<>());
    std::vector<float> all_mask_on_b;
    all_mask_on_b.resize(all_mask.size());
    std::transform(b.at("max").array.begin(), b.at("max").array.end(),
                   all_mask.begin(), all_mask_on_b.begin(),
                   std::multiplies<>());

    // The cells are marked 'true' when there is a change between the two
    // point clouds.
    std::vector<bool> change_mask;
    change_mask.resize(all_mask_on_a.size());
    std::transform(
        all_mask_on_a.begin(), all_mask_on_a.end(), all_mask_on_b.begin(),
        change_mask.begin(),
        [threshold_mutation_difference](const float& a, const float& b) {
          return std::abs(b - a) > threshold_mutation_difference;
        });
    // spdlog::debug("change_mask size: {}, footprint_mask size: {}",
    // change_mask.size(), footprint_mask.size());
    int footprint_pixel_cnt =
        std::accumulate(footprint_mask.begin(), footprint_mask.end(), int(0));
    if (footprint_pixel_cnt == 0) return 0;

    int change_pixel_cnt =
        std::accumulate(change_mask.begin(), change_mask.end(), int(0));

    return (float(change_pixel_cnt) / float(footprint_pixel_cnt)) >=
           threshold_mutation_fraction;
  }

}  // namespace roofer::misc
