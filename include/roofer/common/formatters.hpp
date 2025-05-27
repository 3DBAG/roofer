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

// Formatter for roofer::TBox<double>
template <>
struct std::formatter<roofer::TBox<double>> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::TBox<double>& box, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "[{},{},{},{}]", box.pmin[0], box.pmin[1],
                          box.pmax[0], box.pmax[1]);
  }
};

// Formatter for std::optional<roofer::TBox<double>>
template <>
struct std::formatter<std::optional<roofer::TBox<double>>> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const std::optional<roofer::TBox<double>>& box,
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
