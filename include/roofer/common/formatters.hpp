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

#include <roofer/common/common.hpp>
#include <format>
#include "common.hpp"

// Formatter for roofer::TBox<double>
template <typename T>
struct std::formatter<roofer::TBox<T>> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::TBox<T>& box, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "[{},{},{},{}]", box.pmin[0], box.pmin[1],
                          box.pmax[0], box.pmax[1]);
  }
};

// Formatter for std::optional<roofer::TBox<double>>
template <typename T>
struct std::formatter<std::optional<roofer::TBox<T>>> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const std::optional<roofer::TBox<T>>& box,
              std::format_context& ctx) const {
    if (!box.has_value()) {
      return std::format_to(ctx.out(), "");
    } else {
      return std::format_to(ctx.out(), "[{},{},{},{}]", box->pmin[0],
                            box->pmin[1], box->pmax[0], box->pmax[1]);
    }
  }
};

// Formatter for roofer::arr2f
template <>
struct std::formatter<roofer::arr2f> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::arr2f& arr, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "[{},{}]", arr[0], arr[1]);
  }
};

// Formatter for roofer::arr3d
template <>
struct std::formatter<roofer::arr3d> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::arr3d& arr, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "[{},{},{}]", arr[0], arr[1], arr[2]);
  }
};

// Formatter for std::optional<roofer::arr3d>
template <>
struct std::formatter<std::optional<roofer::arr3d>> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const std::optional<roofer::arr3d>& arr,
              std::format_context& ctx) const {
    if (!arr.has_value()) {
      return std::format_to(ctx.out(), "");
    } else {
      return std::format_to(ctx.out(), "[{},{},{}]", (*arr)[0], (*arr)[1],
                            (*arr)[2]);
    }
  }
};

// Formatter for LinearRingWithOffset as WKT, includes interior rings. Eg a WKT
// with one interior ring would look like: 	POLYGON ((35 10, 45 45, 15 40,
// 10 20, 35 10), (20 30, 35 35, 30 20, 20 30)) attempts to use the
// std::weak_ptr<arr3d> data_offset member
template <>
struct std::formatter<roofer::LinearRing> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::LinearRing& lr, std::format_context& ctx) const {
    std::string wkt = "POLYGON ((";
    for (size_t i = 0; i < lr.size(); ++i) {
      wkt += std::format("{:.6f} {:.6f}", lr[i][0], lr[i][1]);
      if (i < lr.size() - 1) {
        wkt += ", ";
      }
    }
    wkt += ")";
    for (const auto& interior : lr.interior_rings()) {
      wkt += ", (";
      for (size_t i = 0; i < interior.size(); ++i) {
        wkt += std::format("{:.6f} {:.6f}", interior[i][0], interior[i][1]);
        if (i < interior.size() - 1) {
          wkt += ", ";
        }
      }
      wkt += ")";
    }
    wkt += ")";
    return std::format_to(ctx.out(), "{}", wkt);
  }
};

// Formatter for LinearRingWithOffset as WKT, includes interior rings. Eg a WKT
// with one interior ring would look like: 	POLYGON ((35 10, 45 45, 15 40,
// 10 20, 35 10), (20 30, 35 35, 30 20, 20 30)) attempts to use the
// std::weak_ptr<arr3d> data_offset member
template <>
struct std::formatter<roofer::LinearRingWithOffset> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::LinearRingWithOffset& lr,
              std::format_context& ctx) const {
    std::string wkt = "POLYGON ((";
    for (size_t i = 0; i < lr.size(); ++i) {
      wkt += std::format("{:.6f} {:.6f}", lr[i][0] + lr.data_offset[0],
                         lr[i][1] + lr.data_offset[1]);
      if (i < lr.size() - 1) {
        wkt += ", ";
      }
    }
    wkt += ")";
    for (const auto& interior : lr.interior_rings()) {
      wkt += ", (";
      for (size_t i = 0; i < interior.size(); ++i) {
        wkt += std::format("{:.6f} {:.6f}", interior[i][0] + lr.data_offset[0],
                           interior[i][1] + lr.data_offset[1]);
        if (i < interior.size() - 1) {
          wkt += ", ";
        }
      }
      wkt += ")";
    }
    wkt += ")";
    return std::format_to(ctx.out(), "{}", wkt);
  }
};
