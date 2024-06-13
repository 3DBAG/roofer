#pragma once

#include <algorithm>
#include <coroutine>
#include <span>
#include <vector>

#include "datastructures.h"
#include "libfork/core.hpp"
#include "spdlog/spdlog.h"

const auto SLEEP_PER_BUILDING = std::chrono::milliseconds(60);

struct GenerateModelsBatch {
  struct promise_type;

  using Handle = std::coroutine_handle<promise_type>;
  Handle mCoroHdl{};

  explicit GenerateModelsBatch(promise_type* p)
      : mCoroHdl{Handle::from_promise(*p)} {}
  GenerateModelsBatch(GenerateModelsBatch&& rhs) noexcept
      : mCoroHdl{std::exchange(rhs.mCoroHdl, nullptr)} {}

  ~GenerateModelsBatch() {
    if (mCoroHdl) {
      mCoroHdl.destroy();
    }
  }
};

struct GenerateModelsBatch::promise_type {
  std::vector<Points> _valueOut{};

  void unhandled_exception() noexcept {
    spdlog::get("coro")->debug(
        "GenerateModelsBatch::promise_type::unhandled_exception");
  }

  GenerateModelsBatch get_return_object() { return GenerateModelsBatch{this}; }

  std::suspend_never initial_suspend() noexcept {
    spdlog::get("coro")->debug(
        "GenerateModelsBatch::promise_type::initial_suspend");
    return {};
  }

  void return_value(std::vector<Points> value) noexcept {
    spdlog::get("coro")->debug(
        "GenerateModelsBatch::promise_type::return_value");
    _valueOut = std::move(value);
  }

  std::suspend_always final_suspend() noexcept {
    spdlog::get("coro")->debug(
        "GenerateModelsBatch::promise_type::final_suspend");
    return {};
  }
};

GenerateModelsBatch reconstruct_batch(std::span<Points> points_one_batch);

struct GenerateModels {
  struct promise_type;

  using Handle = std::coroutine_handle<promise_type>;
  Handle mCoroHdl{};

  explicit GenerateModels(promise_type* p)
      : mCoroHdl{Handle::from_promise(*p)} {}
  GenerateModels(GenerateModels&& rhs) noexcept
      : mCoroHdl{std::exchange(rhs.mCoroHdl, nullptr)} {}

  ~GenerateModels() {
    if (mCoroHdl) {
      mCoroHdl.destroy();
    }
  }
};

struct GenerateModels::promise_type {
  Points _valueOut{};

  void unhandled_exception() noexcept {
    spdlog::get("coro")->debug(
        "GenerateModels::promise_type::unhandled_exception");
  }

  GenerateModels get_return_object() { return GenerateModels{this}; }

  std::suspend_never initial_suspend() noexcept {
    spdlog::get("coro")->debug("GenerateModels::promise_type::initial_suspend");
    return {};
  }

  void return_value(Points value) noexcept {
    spdlog::get("coro")->debug("GenerateModels::promise_type::return_value");
    _valueOut = std::move(value);
  }

  std::suspend_always final_suspend() noexcept {
    spdlog::get("coro")->debug("GenerateModels::promise_type::final_suspend");
    return {};
  }
};

GenerateModels reconstruct_one_building_coro(Points points_one_building);

Points reconstruct_one_building(Points& points_one_building);

inline constexpr auto reconstruct_one_building_libfork =
    [](auto reconstruct_one_building_libfork,
       Points points_one_building) -> lf::task<Points> {
  Points building_model;
  for (const auto& p : points_one_building.x) {
    building_model.x.push_back(p * (float)42.0);
    building_model.y.push_back(p * (float)42.0);
    building_model.z.push_back(p * (float)42.0);
  }
  std::this_thread::sleep_for(SLEEP_PER_BUILDING);
  co_return building_model;
};
