// Copyright (c) 2018-2026 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.

// Author(s):
// Ravi Peters

#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace roofer::config {

  enum class Visibility { public_, internal };

  template <typename T>
  using Validator = std::function<std::optional<std::string>(const T&)>;

  template <typename Owner, typename T>
  struct Field {
    std::string_view name;
    std::string_view description;
    T Owner::*member;
    Validator<T> validator;
    Visibility visibility = Visibility::public_;

    [[nodiscard]] std::string toml_name() const {
      std::string result(name);
      std::replace(result.begin(), result.end(), '_', '-');
      return result;
    }
  };

  template <typename Owner, typename T>
  Field(std::string_view, std::string_view, T Owner::*, Validator<T>,
        Visibility) -> Field<Owner, T>;

  template <typename T>
  Validator<T> no_validation() {
    return {};
  }

  template <typename T>
  Validator<T> greater_than(T lower) {
    return [lower](const T& value) -> std::optional<std::string> {
      if (value > lower) return std::nullopt;
      return "must be greater than " + std::to_string(lower);
    };
  }

  template <typename T>
  Validator<T> at_least(T lower) {
    return [lower](const T& value) -> std::optional<std::string> {
      if (value >= lower) return std::nullopt;
      return "must be at least " + std::to_string(lower);
    };
  }

  template <typename T>
  Validator<T> in_range(T lower, T upper) {
    return [lower, upper](const T& value) -> std::optional<std::string> {
      if (value >= lower && value <= upper) return std::nullopt;
      return "must be in [" + std::to_string(lower) + ", " +
             std::to_string(upper) + "]";
    };
  }

  template <typename T>
  Validator<std::pair<T, T>> ordered_range(T lower) {
    return [lower](const std::pair<T, T>& value) -> std::optional<std::string> {
      if (value.first >= lower && value.second >= value.first) {
        return std::nullopt;
      }
      return "must be an ordered range with both values at least " +
             std::to_string(lower);
    };
  }

  template <typename Config, typename Visitor>
  void for_each_field(Config& value, Visitor&& visitor,
                      bool include_internal = false) {
    Config::visit_fields([&](auto field) {
      if (include_internal || field.visibility == Visibility::public_) {
        visitor(field, value.*(field.member));
      }
    });
  }

  template <typename Config, typename Visitor>
  void for_each_field(const Config& value, Visitor&& visitor,
                      bool include_internal = false) {
    Config::visit_fields([&](auto field) {
      if (include_internal || field.visibility == Visibility::public_) {
        visitor(field, value.*(field.member));
      }
    });
  }

  template <typename Config>
  [[nodiscard]] bool has_public_fields() {
    bool result = false;
    Config::visit_fields([&](auto field) {
      if (field.visibility == Visibility::public_) result = true;
    });
    return result;
  }

  /** Find the descriptor for a member declared with ROOFER_CONFIG_MEMBERS. */
  template <typename Owner, typename T>
  [[nodiscard]] Field<Owner, T> field_for(T Owner::*member) {
    std::optional<Field<Owner, T>> result;
    Owner::visit_fields([&](auto field) {
      if constexpr (std::is_same_v<decltype(field.member), T Owner::*>) {
        if (field.member == member) result = field;
      }
    });
    if (!result) {
      throw std::logic_error("Member has no configuration field descriptor");
    }
    return *result;
  }

  template <typename Config>
  [[nodiscard]] std::optional<std::string> validate(const Config& value) {
    std::optional<std::string> error;
    for_each_field(
        value,
        [&](auto field, const auto& field_value) {
          if (!error && field.validator) {
            if (auto message = field.validator(field_value)) {
              error = field.toml_name() + " " + *message;
            }
          }
        },
        true);
    return error;
  }

}  // namespace roofer::config

#define ROOFER_CONFIG_DECLARE(type, name, default_value, description, \
                              validator, visibility)                  \
  type name = default_value;

#define ROOFER_CONFIG_VISIT(type, name, default_value, description, validator, \
                            visibility)                                        \
  visitor(::roofer::config::Field{#name, description, &Self::name, validator,  \
                                  ::roofer::config::Visibility::visibility});

#define ROOFER_CONFIG_MEMBERS(field_list)                       \
  field_list(ROOFER_CONFIG_DECLARE) template <typename Visitor> \
  static void visit_fields(Visitor&& visitor) {                 \
    field_list(ROOFER_CONFIG_VISIT)                             \
  }
