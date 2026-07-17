#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "config.hpp"

#include <filesystem>
#include <fstream>

namespace {
  class TemporaryConfig {
   public:
    explicit TemporaryConfig(std::string_view contents)
        : path_(std::filesystem::temp_directory_path() /
                "roofer-nested-reconstruction-test.toml") {
      std::ofstream stream(path_);
      stream << contents;
    }

    ~TemporaryConfig() {
      std::error_code error;
      std::filesystem::remove(path_, error);
    }

    [[nodiscard]] std::string string() const { return path_.string(); }

   private:
    std::filesystem::path path_;
  };
}  // namespace

TEST_CASE("nested reconstruction TOML overrides deprecated root aliases") {
  TemporaryConfig file(R"(
complexity-factor = 0.1
plane-detect-k = 7
plane-detect-normal-angle = 0.6
lod11-fallback-time = 1234

[reconstruction]
clip-terrain = false

[reconstruction.plane-detector]
plane-neighbour-count = 23
normal-angle-threshold = 30

[reconstruction.arrangement-optimiser]
complexity-factor = 0.7
)");

  RooferConfigHandler handler;
  handler._config_path = file.string();
  handler.parse_config_file();

  CHECK(handler.cfg_.reconstruction.clip_terrain == false);
  CHECK(handler.cfg_.reconstruction.plane_detector.plane_neighbour_count == 23);
  CHECK(handler.cfg_.reconstruction.arrangement_optimiser.complexity_factor ==
        0.7F);
  CHECK(handler.cfg_.reconstruction.plane_detector.normal_angle_threshold ==
        30.0F);
  CHECK(handler._deprecated_lod11_fallback_time == 1234);
  CHECK(handler.cfg_.reconstruction.plane_detector.max_plane_count == 900);
}

TEST_CASE("legacy normal dot-product threshold is converted to degrees") {
  TemporaryConfig file("plane-detect-normal-angle = 0.6\n");

  RooferConfigHandler handler;
  handler._config_path = file.string();
  handler.parse_config_file();

  CHECK(handler.cfg_.reconstruction.plane_detector.normal_angle_threshold ==
        Catch::Approx(53.130102F));
}

TEST_CASE("nested reconstruction TOML errors contain complete dotted paths") {
  SECTION("unknown field") {
    TemporaryConfig file(R"(
[reconstruction.arrangement-snapper]
unknown-setting = true
)");
    RooferConfigHandler handler;
    handler._config_path = file.string();
    CHECK_THROWS_WITH(
        handler.parse_config_file(),
        Catch::Matchers::ContainsSubstring(
            "reconstruction.arrangement-snapper.unknown-setting"));
  }

  SECTION("validation") {
    TemporaryConfig file(R"(
[reconstruction.arrangement-optimiser]
complexity-factor = 1.1
)");
    RooferConfigHandler handler;
    handler._config_path = file.string();
    CHECK_THROWS_WITH(
        handler.parse_config_file(),
        Catch::Matchers::ContainsSubstring(
            "reconstruction.arrangement-optimiser.complexity-factor"));
  }

  SECTION("out-of-range integer") {
    TemporaryConfig file(R"(
[reconstruction.plane-detector]
plane-neighbour-count = 2147483648
)");
    RooferConfigHandler handler;
    handler._config_path = file.string();
    CHECK_THROWS_WITH(
        handler.parse_config_file(),
        Catch::Matchers::ContainsSubstring(
            "reconstruction.plane-detector.plane-neighbour-count"));
  }

  SECTION("out-of-range integer in pair") {
    TemporaryConfig file(R"(
[reconstruction.line-detector]
min-point-count-range = [5, 2147483648]
)");
    RooferConfigHandler handler;
    handler._config_path = file.string();
    CHECK_THROWS_WITH(
        handler.parse_config_file(),
        Catch::Matchers::ContainsSubstring(
            "reconstruction.line-detector.min-point-count-range"));
  }
}

TEST_CASE("legacy reconstruction aliases reuse field metadata") {
  RooferConfigHandler handler;
  using Optimiser = roofer::reconstruction::ArrangementOptimiserConfig;
  const auto field = roofer::config::field_for(&Optimiser::complexity_factor);
  auto* alias = handler.param_index_.at("complexity-factor");

  CHECK(alias->description() == field.description);

  handler.cfg_.reconstruction.arrangement_optimiser.complexity_factor = 1.1F;
  REQUIRE(field.validator);
  CHECK(alias->validate() == field.validator(1.1F));
}

TEST_CASE("legacy warning destinations include exact nested subsections") {
  const auto complexity =
      legacy_reconstruction_destination("complexity-factor");
  REQUIRE(complexity);
  CHECK(complexity->section == "reconstruction.arrangement-optimiser");
  CHECK(complexity->nested_key == "complexity-factor");
  CHECK_FALSE(complexity->cli_deprecated);

  const auto plane_count =
      legacy_reconstruction_destination("lod11-fallback-planes");
  REQUIRE(plane_count);
  CHECK(plane_count->section == "reconstruction.plane-detector");
  CHECK(plane_count->nested_key == "max-plane-count");
  CHECK(plane_count->cli_deprecated);

  for (const std::string_view stable_cli_option :
       {"lod12", "lod13", "lod22", "clip-terrain", "complexity-factor"}) {
    const auto destination =
        legacy_reconstruction_destination(stable_cli_option);
    REQUIRE(destination);
    CHECK_FALSE(destination->cli_deprecated);
  }

  const auto removed_time =
      legacy_reconstruction_destination("lod11-fallback-time");
  REQUIRE(removed_time);
  CHECK(removed_time->section == "reconstruction.plane-detector");
  CHECK(removed_time->nested_key == "max-plane-count");
  CHECK(removed_time->cli_deprecated);
  CHECK(removed_time->ignored);

  const auto root_only =
      legacy_reconstruction_destination("plane-detect-normal-angle");
  REQUIRE(root_only);
  CHECK_FALSE(root_only->cli_supported);

  RooferConfigHandler handler;
  CHECK_FALSE(handler.param_index_.contains("plane-detect-normal-angle"));
  CHECK(handler.root_only_reconstruction_alias_index_.contains(
      "plane-detect-normal-angle"));
}

TEST_CASE("public reconstruction CLI flags remain supported") {
  const char* argv[] = {"roofer",     "--lod12",
                        "--no-lod22", "--complexity-factor",
                        "0.4",        "--no-clip-terrain"};
  CLIArgs arguments(static_cast<int>(std::size(argv)), argv);
  RooferConfigHandler handler;
  handler.cfg_.source_footprints = "footprints.gpkg";
  handler.cfg_.output_path = "output";
  handler.input_pointclouds_.emplace_back();

  handler.parse_cli_second_pass(arguments);

  CHECK(handler.cfg_.reconstruction.lod12);
  CHECK_FALSE(handler.cfg_.reconstruction.lod22);
  CHECK(handler.cfg_.reconstruction.arrangement_optimiser.complexity_factor ==
        0.4F);
  CHECK_FALSE(handler.cfg_.reconstruction.clip_terrain);
  CHECK(arguments.args.empty());
}

TEST_CASE("removed fallback time option is accepted and ignored") {
  const char* argv[] = {"roofer", "--lod11-fallback-time", "1234"};
  CLIArgs arguments(static_cast<int>(std::size(argv)), argv);
  RooferConfigHandler handler;
  handler.cfg_.source_footprints = "footprints.gpkg";
  handler.cfg_.output_path = "output";
  handler.input_pointclouds_.emplace_back();

  handler.parse_cli_second_pass(arguments);

  CHECK(handler._deprecated_lod11_fallback_time == 1234);
  CHECK(handler.cfg_.reconstruction.plane_detector.max_plane_count == 900);
  CHECK(arguments.args.empty());
}
