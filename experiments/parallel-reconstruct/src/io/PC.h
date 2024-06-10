#pragma once

#include <coroutine>
#include <cstdlib>
#include <iostream>
#include <ranges>

#include "datastructures.h"
#include "spdlog/spdlog.h"

struct ReturnPoints {
  struct promise_type;

  using Handle = std::coroutine_handle<promise_type>;
  Handle mCoroHdl{};

  explicit ReturnPoints(promise_type* p) : mCoroHdl{Handle::from_promise(*p)} {}
  ReturnPoints(ReturnPoints&& rhs) noexcept
      : mCoroHdl{std::exchange(rhs.mCoroHdl, nullptr)} {}

  ~ReturnPoints() {
    if (mCoroHdl) {
      mCoroHdl.destroy();
    }
  }
};

struct ReturnPoints::promise_type {
  Points _valueOut{};

  void unhandled_exception() noexcept {
    spdlog::get("read_pc")->debug(
        "ReturnPoints::promise_type::unhandled_exception");
  }

  ReturnPoints get_return_object() { return ReturnPoints{this}; }

  std::suspend_never initial_suspend() noexcept {
    spdlog::get("read_pc")->debug(
        "ReturnPoints::promise_type::initial_suspend");
    return {};
  }

  auto return_value(Points value) noexcept {
    spdlog::get("read_pc")->debug("ReturnPoints::promise_type::return_value");
    _valueOut = std::move(value);
  }

  std::suspend_always final_suspend() noexcept {
    spdlog::get("read_pc")->debug("ReturnPoints::promise_type::final_suspend");
    return {};
  }
};

ReturnPoints read_pointcloud_coro(uint nr_points = 0);

Points read_pointcloud(uint nr_points = 0);
