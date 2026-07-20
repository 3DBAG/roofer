// Copyright (c) 2018-2025 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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
// Balázs Dukai
#pragma once
#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <vector>

namespace roofer::validators {
  // Array-specific application validation; scalar bounds use roofer::config.
  inline auto AllHigherThan(roofer::arr2f min) {
    return [min](const roofer::arr2f& val) -> std::optional<std::string> {
      if (!std::isfinite(val[0]) || !std::isfinite(val[1])) {
        return "Values must be finite.";
      }
      if (val[0] <= min[0] || val[1] <= min[1]) {
        return std::format(
            "One of the values of [{}, {}] is too low. Values must be higher "
            "than {} and {} respectively.",
            val[0], val[1], min[0], min[1]);
      }
      return std::nullopt;
    };
  }

  // Generator function for validator to check if the value is one of the given
  // values
  template <typename T>
  auto OneOf(std::vector<T> values) {
    return [values](const T& val) -> std::optional<std::string> {
      if (std::find(values.begin(), values.end(), val) == values.end()) {
        return std::format("Value {} is not one of the allowed values.", val);
      }
      return std::nullopt;
    };
  };

  // Box validator
  auto ValidBox =
      [](const roofer::TBox<double>& box) -> std::optional<std::string> {
    if (!std::isfinite(box.pmin[0]) || !std::isfinite(box.pmin[1]) ||
        !std::isfinite(box.pmax[0]) || !std::isfinite(box.pmax[1])) {
      return "Box coordinates must be finite.";
    }
    if (box.pmin[0] >= box.pmax[0] || box.pmin[1] >= box.pmax[1]) {
      return "Box is invalid.";
    }
    return std::nullopt;
  };
}  // namespace roofer::validators
