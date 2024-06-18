#pragma once

#include <coroutine>
#include <queue>

#include "PC.h"
#include "datastructures.h"
#include "libfork/core.hpp"

struct GenerateCroppedPointsBatch {
  struct promise_type;

  using Handle = std::coroutine_handle<promise_type>;
  Handle mCoroHdl{};

  explicit GenerateCroppedPointsBatch(promise_type* p)
      : mCoroHdl{Handle::from_promise(*p)} {}
  GenerateCroppedPointsBatch(GenerateCroppedPointsBatch&& rhs) noexcept
      : mCoroHdl{std::exchange(rhs.mCoroHdl, nullptr)} {}

  ~GenerateCroppedPointsBatch() {
    if (mCoroHdl) {
      mCoroHdl.destroy();
    }
  }
};

struct GenerateCroppedPointsBatch::promise_type {
  std::vector<Points> _valueOut{};

  void unhandled_exception() noexcept {
    spdlog::get("coro")->debug(
        "GenerateCroppedPointsBatch::promise_type::unhandled_exception");
  }

  GenerateCroppedPointsBatch get_return_object() {
    return GenerateCroppedPointsBatch{this};
  }

  std::suspend_never initial_suspend() noexcept {
    spdlog::get("coro")->debug(
        "GenerateCroppedPointsBatch::promise_type::initial_suspend");
    return {};
  }

  std::suspend_always yield_value(std::vector<Points> value) noexcept {
    spdlog::get("coro")->debug(
        "GenerateCroppedPointsBatch::promise_type::yield_value");
    _valueOut = std::move(value);
    return {};
  }

  std::suspend_always final_suspend() noexcept {
    spdlog::get("coro")->debug(
        "GenerateCroppedPointsBatch::promise_type::final_suspend");
    return {};
  }
};

GenerateCroppedPointsBatch crop_coro_batch(uint nr_points_per_laz, uint nr_laz);

struct GenerateCroppedPoints {
  struct promise_type;

  using Handle = std::coroutine_handle<promise_type>;
  Handle mCoroHdl{};

  explicit GenerateCroppedPoints(promise_type* p)
      : mCoroHdl{Handle::from_promise(*p)} {}
  GenerateCroppedPoints(GenerateCroppedPoints&& rhs) noexcept
      : mCoroHdl{std::exchange(rhs.mCoroHdl, nullptr)} {}

  ~GenerateCroppedPoints() {
    if (mCoroHdl) {
      mCoroHdl.destroy();
    }
  }
};

struct GenerateCroppedPoints::promise_type {
  Points _valueOut{};

  void unhandled_exception() noexcept {
    spdlog::get("coro")->debug(
        "GenerateCroppedPoints::promise_type::unhandled_exception");
  }

  GenerateCroppedPoints get_return_object() {
    return GenerateCroppedPoints{this};
  }

  std::suspend_never initial_suspend() noexcept {
    spdlog::get("coro")->debug(
        "GenerateCroppedPoints::promise_type::initial_suspend");
    return {};
  }

  std::suspend_always yield_value(Points value) noexcept {
    spdlog::get("coro")->debug(
        "GenerateCroppedPoints::promise_type::yield_value");
    _valueOut = std::move(value);
    return {};
  }

  std::suspend_always final_suspend() noexcept {
    spdlog::get("coro")->debug(
        "GenerateCroppedPoints::promise_type::final_suspend");
    return {};
  }
};

GenerateCroppedPoints crop_coro(uint nr_points_per_laz, uint nr_laz);

void crop(uint nr_points_per_laz, uint nr_laz,
          std::queue<Points>& queue_cropped);
void crop(uint nr_points_per_laz, uint nr_laz,
          lf::deque<DequePoints>& queue_cropped);
