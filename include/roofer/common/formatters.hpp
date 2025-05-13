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

// Formatter for roofer::enum::TerrainStrategy
template <>
struct std::formatter<roofer::enums::TerrainStrategy> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::enums::TerrainStrategy& strategy,
              std::format_context& ctx) const {
    switch (strategy) {
      case roofer::enums::BUFFER_TILE:
        return std::format_to(ctx.out(), "buffer_tile");
      case roofer::enums::BUFFER_USER:
        return std::format_to(ctx.out(), "buffer_user");
      case roofer::enums::USER:
        return std::format_to(ctx.out(), "user");
      default:
        return std::format_to(ctx.out(), "unknown");
    }
  }
};

// Formatter for roofer::logger::LogLevel
template <>
struct std::formatter<roofer::logger::LogLevel> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::logger::LogLevel& level,
              std::format_context& ctx) const {
    switch (level) {
      case roofer::logger::LogLevel::trace:
        return std::format_to(ctx.out(), "trace");
      case roofer::logger::LogLevel::debug:
        return std::format_to(ctx.out(), "debug");
      case roofer::logger::LogLevel::info:
        return std::format_to(ctx.out(), "info");
      default:
        return std::format_to(ctx.out(), "unknown");
    }
  }
};
