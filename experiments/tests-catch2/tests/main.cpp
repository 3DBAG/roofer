#include <catch2/catch_test_macros.hpp>

#include "library.h"

TEST_CASE("Test case long description returns_42", "[brief_returns_42]") {
  REQUIRE(returns_42(0) == 1);
  REQUIRE(returns_42(0) == 42);
}

TEST_CASE("vectors can be sized and resized", "[vector]") {
  // This setup will be done 4 times in total, once for each section
  std::vector<int> v(5);

  REQUIRE(v.size() == 5);
  REQUIRE(v.capacity() >= 5);

  SECTION("resizing bigger changes size and capacity") {
    v.resize(10);

    REQUIRE(v.size() == 10);
    REQUIRE(v.capacity() >= 10);
  }
  SECTION("resizing smaller changes size but not capacity") {
    v.resize(0);

    REQUIRE(v.size() == 0);
    REQUIRE(v.capacity() >= 5);
  }
  SECTION("reserving bigger changes capacity but not size") {
    v.reserve(10);

    REQUIRE(v.size() == 5);
    REQUIRE(v.capacity() >= 10);
  }
  SECTION("reserving smaller does not change size or capacity") {
    v.reserve(0);

    REQUIRE(v.size() == 5);
    REQUIRE(v.capacity() >= 5);
  }
}
