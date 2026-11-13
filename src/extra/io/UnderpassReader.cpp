#include <roofer/io/UnderpassReader.hpp>
#include <roofer/logger/logger.h>
#include <roofer/io/VectorReader.hpp>
#include <cmath>

namespace roofer::io {
  Underpass UnderpassFeature::localize(const arr3d& offset, float floor,
                                       float default_clearance) const {
    Underpass result;
    result.source_id = source_id;
    result.attributes = attributes;
    auto ring = [&](const auto& points) {
      vec3f local;
      for (const auto& p : points)
        local.push_back({float(p[0] - offset[0]), float(p[1] - offset[1]), 0});
      return local;
    };
    auto outer = ring(exterior);
    result.polygon.insert(result.polygon.end(), outer.begin(), outer.end());
    for (const auto& hole : holes)
      result.polygon.interior_rings().push_back(ring(hole));
    result.ceiling_elevation = ceiling_elevation
                                   ? *ceiling_elevation - offset[2]
                                   : double(floor) + default_clearance;
    return result;
  }

  UnderpassIndex readUnderpasses(const std::string& source,
                                 const std::string& layer_name,
                                 const std::string& id_attribute,
                                 const std::string& height_attribute,
                                 const std::string& expected_crs,
                                 const std::vector<std::string>& attributes) {
    auto coordinates = misc::createProjHelper();
    auto reader = createVectorReaderOGR(*coordinates);
    reader->layer_name = layer_name;
    reader->strict_layer_name = true;
    reader->skip_invalid_polygons = true;
    reader->skip_unsupported_geometries = true;
    reader->expected_crs = expected_crs;
    reader->open(source);
    std::vector<PolygonFeature> polygons;
    reader->readPolygons(polygons, {id_attribute},
                         height_attribute.empty()
                             ? std::vector<std::string>{}
                             : std::vector<std::string>{height_attribute},
                         attributes);
    auto ring = [](const std::vector<arr3d>& input) {
      std::vector<arr2d> result;
      for (const auto& p : input) result.push_back({p[0], p[1]});
      return result;
    };
    UnderpassIndex result;
    auto& log = logger::Logger::get_logger();
    for (const auto& polygon : polygons) {
      const auto& id = polygon.text_attributes.at(id_attribute);
      if (!id || id->empty()) {
        log.warning("Skipping underpass feature {}: missing or empty ID",
                    polygon.fid);
        continue;
      }
      std::optional<double> height;
      if (!height_attribute.empty()) {
        height = polygon.numeric_attributes.at(height_attribute);
        if (height && !std::isfinite(*height)) {
          log.warning(
              "Skipping underpass feature {} for {}: invalid ceiling elevation",
              polygon.fid, *id);
          continue;
        }
      }
      UnderpassFeature value;
      value.exterior = ring(polygon.exterior);
      for (const auto& hole : polygon.holes) value.holes.push_back(ring(hole));
      value.ceiling_elevation = height;
      value.source_id = *id + "/FID " + std::to_string(polygon.fid);
      value.attributes = polygon.attributes;
      result[*id].push_back(std::move(value));
    }
    return result;
  }
}  // namespace roofer::io
