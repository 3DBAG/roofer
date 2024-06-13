#pragma once

#include <array>
#include <atomic>
#include <span>
#include <vector>

using arr3f = std::array<float, 3>;
using vec3f = std::vector<float>;
using a_span_f = std::atomic<std::span<float>>;

struct Points {
  vec3f x{};
  vec3f y{};
  vec3f z{};
};

// Cannot use std::vector with libfork::deque, because the deque type must be
// trivially copyable
// (https://en.cppreference.com/w/cpp/types/is_trivially_copyable)
using DequePoints = float;
