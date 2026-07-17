#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <roofer/reconstruction/ReconstructionConfig.hpp>
#include <roofer/roofer.h>

#include <cmath>
#include <numbers>
#include <set>
#include <string>

using roofer::config::for_each_field;
using roofer::reconstruction::ReconstructionConfig;

TEST_CASE("reconstruction descriptor defaults are canonical") {
  ReconstructionConfig cfg;

  CHECK(cfg.plane_detector.plane_neighbour_count == 15);
  CHECK(cfg.plane_detector.min_plane_points == 15);
  CHECK(cfg.plane_detector.plane_epsilon == Catch::Approx(0.3F));
  CHECK(cfg.plane_detector.normal_angle_threshold == Catch::Approx(41.409622F));
  CHECK(cfg.plane_detector.horizontal_angle_threshold ==
        Catch::Approx(5.731968F));
  CHECK(cfg.plane_detector.wall_angle_threshold == Catch::Approx(72.542397F));
  CHECK(cfg.line_regulariser.angle_threshold == Catch::Approx(8.594367F));
  CHECK(cfg.plane_detector.max_plane_count == 900);
  CHECK(cfg.arrangement_optimiser.complexity_factor == Catch::Approx(0.888F));
  CHECK(cfg.arrangement_snapper.distance_threshold == Catch::Approx(0.021F));
  CHECK(cfg.arrangement_builder.snap_tolerance == Catch::Approx(0.01F));
  CHECK(cfg.arrangement_extruder.snap_tolerance == Catch::Approx(0.001258925F));
  CHECK(cfg.mesh_triangulator.duplicate_tolerance == Catch::Approx(0.0001F));
  CHECK_FALSE(cfg.validate().has_value());

  const auto cosine = [](float degrees) {
    return std::cos(degrees * std::numbers::pi_v<double> / 180.0);
  };
  CHECK(cosine(cfg.plane_detector.normal_angle_threshold) ==
        Catch::Approx(0.75));
  CHECK(cosine(cfg.plane_detector.horizontal_angle_threshold) ==
        Catch::Approx(0.995));
  CHECK(cosine(cfg.plane_detector.wall_angle_threshold) == Catch::Approx(0.3));
  CHECK(cfg.line_regulariser.angle_threshold * std::numbers::pi_v<double> /
            180.0 ==
        Catch::Approx(0.15));
  CHECK(-std::log10(cfg.arrangement_builder.snap_tolerance) ==
        Catch::Approx(2.0));
  CHECK(-std::log10(cfg.arrangement_extruder.snap_tolerance) ==
        Catch::Approx(2.9));
  CHECK(-std::log10(cfg.mesh_triangulator.duplicate_tolerance) ==
        Catch::Approx(4.0));
}

TEST_CASE("reconstruction angle fields are bounded degree values") {
  ReconstructionConfig cfg;
  cfg.plane_detector.normal_angle_threshold = 90.1F;
  auto error = cfg.validate();
  REQUIRE(error);
  CHECK(error->starts_with("plane-detector.normal-angle-threshold"));

  cfg = {};
  cfg.line_regulariser.angle_threshold = 180.1F;
  error = cfg.validate();
  REQUIRE(error);
  CHECK(error->starts_with("line-regulariser.angle-threshold"));
}

TEST_CASE("unit scale converts metre distances to input coordinate units") {
  ReconstructionConfig cfg;
  cfg.unit_scale = 0.3048F;  // feet
  const auto scaled = cfg.scaled_to_input_units();

  CHECK(scaled.unit_scale == Catch::Approx(1.0F));
  CHECK(scaled.plane_detector.plane_epsilon == Catch::Approx(0.984252F));
  CHECK(scaled.alpha_shaper.alpha ==
        Catch::Approx(0.25F / (0.3048F * 0.3048F)));
  CHECK(scaled.segment_rasteriser.cell_size == Catch::Approx(0.164042F));
  CHECK(scaled.arrangement_extruder.snap_tolerance ==
        Catch::Approx(0.004130331F));

  cfg.unit_scale = 0.0F;
  CHECK(cfg.validate()->starts_with("unit-scale"));
}

TEST_CASE("descriptor names convert to kebab case and hide derived fields") {
  ReconstructionConfig cfg;
  std::set<std::string> optimiser_fields;
  for_each_field(cfg.arrangement_optimiser, [&](auto field, const auto&) {
    optimiser_fields.insert(field.toml_name());
  });

  CHECK(optimiser_fields.contains("complexity-factor"));
  CHECK_FALSE(optimiser_fields.contains("label-ground-outside-footprint"));
  CHECK_FALSE(optimiser_fields.contains("use-ground"));
}

TEST_CASE("components report whether they contain public fields") {
  CHECK(roofer::config::has_public_fields<
        roofer::reconstruction::ArrangementOptimiserConfig>());
  CHECK_FALSE(roofer::config::has_public_fields<
              roofer::reconstruction::ArrangementDissolverConfig>());
}

TEST_CASE("field descriptors can be recovered from their member") {
  using Optimiser = roofer::reconstruction::ArrangementOptimiserConfig;
  const auto field = roofer::config::field_for(&Optimiser::complexity_factor);

  CHECK(field.toml_name() == "complexity-factor");
  REQUIRE(field.validator);
  CHECK(field.validator(1.1F).has_value());
}

TEST_CASE("nested validation reports the complete component path") {
  ReconstructionConfig cfg;
  cfg.arrangement_optimiser.complexity_factor = 1.01F;
  auto error = cfg.validate();
  REQUIRE(error);
  CHECK(error->starts_with("arrangement-optimiser.complexity-factor"));

  cfg = {};
  cfg.line_detector.min_point_count_range = {10, 5};
  error = cfg.validate();
  REQUIRE(error);
  CHECK(error->starts_with("line-detector.min-point-count-range"));
}

TEST_CASE("all reconstruction components are enumerated") {
  ReconstructionConfig cfg;
  std::set<std::string> names;
  cfg.visit_components(
      [&](std::string_view name, const auto&) { names.emplace(name); });

  CHECK(names.size() == 12);
  CHECK(names.contains("plane-detector"));
  CHECK(names.contains("arrangement-optimiser"));
  CHECK(names.contains("mesh-triangulator"));
}

TEST_CASE("complexity factor exclusively balances both energy terms") {
  roofer::reconstruction::ArrangementOptimiserConfig optimiser;
  CHECK(optimiser.data_weight() == Catch::Approx(0.888F));
  CHECK(optimiser.smoothness_weight() == Catch::Approx(0.112F));

  optimiser.complexity_factor = 1.0F;
  CHECK(optimiser.data_weight() == Catch::Approx(1.0F));
  CHECK(optimiser.smoothness_weight() == Catch::Approx(0.0F));

  optimiser.complexity_factor = 0.0F;
  CHECK(optimiser.data_weight() == Catch::Approx(0.0F));
  CHECK(optimiser.smoothness_weight() == Catch::Approx(1.0F));
}

TEST_CASE("flattened API validation delegates to nested descriptors") {
  roofer::ReconstructionConfig legacy;
  CHECK(legacy.is_valid());
  CHECK(legacy.plane_detect_normal_angle == Catch::Approx(0.75F));
  CHECK(roofer::to_reconstruct_options(legacy)
            .reconstruction.plane_detector.normal_angle_threshold ==
        Catch::Approx(41.409622F));

  legacy.unit_scale = 0.3048F;
  CHECK(roofer::to_reconstruct_options(legacy).reconstruction.unit_scale ==
        Catch::Approx(0.3048F));

  legacy.plane_detect_normal_angle = 0.6F;
  CHECK(roofer::to_reconstruct_options(legacy)
            .reconstruction.plane_detector.normal_angle_threshold ==
        Catch::Approx(53.130102F));

  legacy.plane_detect_min_points = 2;
  CHECK_FALSE(legacy.is_valid());
}
