#pragma once

#include <algorithm>
#include <coroutine>
#include <vector>

#include "datastructures.h"
#include "spdlog/spdlog.h"

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

GenerateModels reconstruct_one_building_coro(Points& points_one_building);

Points reconstruct_one_building(Points& points_one_building);
