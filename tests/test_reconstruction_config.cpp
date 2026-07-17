#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <roofer/reconstruction/ReconstructionConfig.hpp>
#include <roofer/roofer.h>

#include <set>
#include <string>

using roofer::config::for_each_field;
using roofer::reconstruction::ReconstructionConfig;

TEST_CASE("reconstruction descriptor defaults are canonical") {
  ReconstructionConfig cfg;

  CHECK(cfg.plane_detector.plane_neighbour_count == 15);
  CHECK(cfg.plane_detector.min_plane_points == 15);
  CHECK(cfg.plane_detector.plane_epsilon == Catch::Approx(0.3F));
  CHECK(cfg.plane_detector.max_plane_count == 900);
  CHECK(cfg.arrangement_optimiser.complexity_factor == Catch::Approx(0.888F));
  CHECK(cfg.arrangement_snapper.distance_threshold == Catch::Approx(0.021F));
  CHECK_FALSE(cfg.validate().has_value());
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

  legacy.plane_detect_min_points = 2;
  CHECK_FALSE(legacy.is_valid());
}
