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

#include "ElevationProvider.hpp"

#include "cdt_util.hpp"

namespace roofer::detection {

  struct ConstantElevationProvider : public ElevationProvider {
    const float floor_elevation_;

    ConstantElevationProvider(const float floor_elevation)
        : floor_elevation_(floor_elevation) {};

    virtual float get(const Point_2 /* pt */) const override {
      return floor_elevation_;
    }

    virtual float get_percentile(float /* percentile */) const override {
      return floor_elevation_;
    }

  };

  struct InterpolatedElevationProvider : public ElevationProvider {
    std::shared_ptr<const proj_tri_util::DT> base_cdt_ptr_;

    InterpolatedElevationProvider(const proj_tri_util::DT& base_cdt)
        : base_cdt_ptr_(std::make_shared<const proj_tri_util::DT>(base_cdt)) {};

    virtual float get(const Point_2 pt) const override {
      return proj_tri_util::interpolate_from_cdt(pt, *base_cdt_ptr_);
    }

    virtual float get_percentile(float percentile) const override {
      std::vector<float> elevations;
      // insert vertex elevation into the sorted vector
      auto insertSorted = [&elevations](float elevation) {
        auto position =
            std::lower_bound(elevations.begin(), elevations.end(), elevation);
        elevations.insert(position, elevation);
      };
      // iterate over all vertices and insert elevation
      for (auto& pt : base_cdt_ptr_->points())
        insertSorted(pt.z());
      // return percentile
      return compute_percentile(elevations, percentile);
    }

    private:
      float compute_percentile(std::vector<float>& z_vec, float percentile) const {
        assert(percentile<=1.);
        assert(percentile>=0.);
        size_t n = (z_vec.size()-1) * percentile;
        std::nth_element(z_vec.begin(), z_vec.begin()+n, z_vec.end());
        return z_vec[n];
      }

  };

  std::unique_ptr<ElevationProvider> createElevationProvider(const float floor_elevation) {
    return std::make_unique<ConstantElevationProvider>(floor_elevation);
  }

  std::unique_ptr<ElevationProvider> createElevationProvider(const proj_tri_util::DT& base_cdt) {
    return std::make_unique<InterpolatedElevationProvider>(base_cdt);
  }

} // namespace roofer::detection
