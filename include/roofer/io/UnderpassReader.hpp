#pragma once

#include <roofer/reconstruction/Underpass.hpp>
#include <optional>
#include <unordered_map>

namespace roofer::io {
  /** Source coordinates stay double precision until a tile origin is known. */
  struct UnderpassFeature {
    std::vector<arr2d> exterior;
    std::vector<std::vector<arr2d>> holes;
    std::optional<double> ceiling_elevation;
    std::string source_id;
    AttributeMapRow attributes;

    Underpass localize(const arr3d& offset, float local_floor_elevation,
                       float default_clearance) const;
  };
  using UnderpassIndex =
      std::unordered_map<std::string, std::vector<UnderpassFeature>>;

  // No spatial/centroid filter: building ID determines ownership across tiles.
  // An empty height attribute or a null value selects the caller's default
  // floor clearance. Selected attributes must exist in the source layer and
  // are passed through unchanged to the localized underpass.
  UnderpassIndex readUnderpasses(
      const std::string& source, const std::string& layer,
      const std::string& id_attribute, const std::string& height_attribute,
      const std::string& expected_crs = {},
      const std::vector<std::string>& attributes = {});
}  // namespace roofer::io
