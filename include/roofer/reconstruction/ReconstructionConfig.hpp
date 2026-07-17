// Copyright (c) 2018-2026 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ravi Peters

#pragma once

#include <roofer/reconstruction/AlphaShaper.hpp>
#include <roofer/reconstruction/ArrangementBuilder.hpp>
#include <roofer/reconstruction/ArrangementDissolver.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include <roofer/reconstruction/ArrangementOptimiser.hpp>
#include <roofer/reconstruction/ArrangementSnapper.hpp>
#include <roofer/reconstruction/LineDetector.hpp>
#include <roofer/reconstruction/LineRegulariser.hpp>
#include <roofer/reconstruction/MeshTriangulator.hpp>
#include <roofer/reconstruction/PlaneDetector.hpp>
#include <roofer/reconstruction/PlaneIntersector.hpp>
#include <roofer/reconstruction/SegmentRasteriser.hpp>

namespace roofer::enums {
  enum TerrainStrategy { BUFFER_TILE = 0, BUFFER_USER = 1, USER = 2 };
}

namespace roofer::reconstruction {

#define ROOFER_RECONSTRUCTION_ROOT_FIELDS(X)                             \
  X(bool, lod12, false, "Generate LoD 1.2 geometry.",                    \
    config::no_validation<bool>(), public_)                              \
  X(bool, lod13, false, "Generate LoD 1.3 geometry.",                    \
    config::no_validation<bool>(), public_)                              \
  X(bool, lod22, true, "Generate LoD 2.2 geometry.",                     \
    config::no_validation<bool>(), public_)                              \
  X(bool, clip_terrain, true,                                            \
    "Clip roofprints where patches of terrain points are detected.",     \
    config::no_validation<bool>(), public_)                              \
  X(float, lod13_step_height, 3.0F,                                      \
    "Maximum step height dissolved during LoD 1.3 generalisation, in "   \
    "metres.",                                                           \
    config::greater_than(0.0F), public_)                                 \
  X(float, unit_scale, 1.0F,                                             \
    "Metres per input coordinate unit. Distance parameters specified "   \
    "in metres are converted to input units for reconstruction using "   \
    "this factor. E.g. for feet use 0.3048.",                            \
    config::greater_than(0.0F), internal)                                \
  X(roofer::enums::TerrainStrategy, h_terrain_strategy,                  \
    roofer::enums::BUFFER_TILE, "Terrain elevation selection strategy.", \
    config::no_validation<roofer::enums::TerrainStrategy>(), public_)

#define ROOFER_RECONSTRUCTION_COMPONENTS(X)                                  \
  X(PlaneDetectorConfig, plane_detector, "plane-detector")                   \
  X(AlphaShaperConfig, alpha_shaper, "alpha-shaper")                         \
  X(LineDetectorConfig, line_detector, "line-detector")                      \
  X(PlaneIntersectorConfig, plane_intersector, "plane-intersector")          \
  X(LineRegulariserConfig, line_regulariser, "line-regulariser")             \
  X(SegmentRasteriserConfig, segment_rasteriser, "segment-rasteriser")       \
  X(ArrangementBuilderConfig, arrangement_builder, "arrangement-builder")    \
  X(ArrangementOptimiserConfig, arrangement_optimiser,                       \
    "arrangement-optimiser")                                                 \
  X(ArrangementDissolverConfig, arrangement_dissolver,                       \
    "arrangement-dissolver")                                                 \
  X(ArrangementSnapperConfig, arrangement_snapper, "arrangement-snapper")    \
  X(ArrangementExtruderConfig, arrangement_extruder, "arrangement-extruder") \
  X(MeshTriangulatorConfig, mesh_triangulator, "mesh-triangulator")

#define ROOFER_DECLARE_COMPONENT(type, member, key) type member;
#define ROOFER_VISIT_COMPONENT(type, member, key) visitor(key, member);
#define ROOFER_VISIT_COMPONENT_FIELD(type, member, key) \
  visitor(key, &Self::member);

  /** Configuration for every stage of the reconstruction pipeline. */
  struct ReconstructionConfig {
    using Self = ReconstructionConfig;
    ROOFER_CONFIG_MEMBERS(ROOFER_RECONSTRUCTION_ROOT_FIELDS)

    ROOFER_RECONSTRUCTION_COMPONENTS(ROOFER_DECLARE_COMPONENT)

    template <typename Visitor>
    void visit_components(Visitor&& visitor) {
      ROOFER_RECONSTRUCTION_COMPONENTS(ROOFER_VISIT_COMPONENT)
    }

    template <typename Visitor>
    void visit_components(Visitor&& visitor) const {
      ROOFER_RECONSTRUCTION_COMPONENTS(ROOFER_VISIT_COMPONENT)
    }

    template <typename Visitor>
    static void visit_component_fields(Visitor&& visitor) {
      ROOFER_RECONSTRUCTION_COMPONENTS(ROOFER_VISIT_COMPONENT_FIELD)
    }

    [[nodiscard]] std::optional<std::string> validate() const {
      if (auto error = config::validate(*this)) return error;
      std::optional<std::string> result;
      visit_components([&](std::string_view name, const auto& component) {
        if (!result) {
          if (auto error = config::validate(component)) {
            result = std::string(name) + "." + *error;
          }
        }
      });
      return result;
    }

    /**
     * Convert metre-based parameters to input coordinate units. `unit_scale`
     * is the number of metres per input coordinate unit.
     */
    [[nodiscard]] Self scaled_to_input_units() const {
      Self result = *this;
      const float distance_scale = 1.0F / unit_scale;
      const float area_scale = distance_scale * distance_scale;

      result.lod13_step_height *= distance_scale;
      result.plane_detector.plane_epsilon *= distance_scale;
      result.plane_detector.ransac_cluster_epsilon *= distance_scale;
      result.plane_detector.maximum_offset *= distance_scale;
      result.alpha_shaper.alpha *= area_scale;
      result.line_detector.distance_threshold *= distance_scale;
      result.line_detector.snap_threshold *= distance_scale;
      result.line_detector.extension *= distance_scale;
      result.plane_intersector.distance_to_line_threshold *= distance_scale;
      result.plane_intersector.min_length *= distance_scale;
      result.line_regulariser.distance_threshold *= distance_scale;
      result.line_regulariser.extension *= distance_scale;
      result.segment_rasteriser.cell_size *= distance_scale;
      result.arrangement_builder.snap_tolerance *= distance_scale;
      result.arrangement_builder.footprint_extension *= distance_scale;
      result.arrangement_dissolver.step_height_threshold *= distance_scale;
      result.arrangement_snapper.distance_threshold *= distance_scale;
      result.arrangement_snapper.manifold_repair_radius *= distance_scale;
      result.arrangement_snapper.manifold_height_tolerance *= distance_scale;
      result.arrangement_extruder.nodata_elevation *= distance_scale;
      result.arrangement_extruder.snap_tolerance *= distance_scale;
      result.mesh_triangulator.duplicate_tolerance *= distance_scale;
      result.unit_scale = 1.0F;
      return result;
    }
  };

#undef ROOFER_RECONSTRUCTION_ROOT_FIELDS
#undef ROOFER_RECONSTRUCTION_COMPONENTS
#undef ROOFER_DECLARE_COMPONENT
#undef ROOFER_VISIT_COMPONENT
#undef ROOFER_VISIT_COMPONENT_FIELD

}  // namespace roofer::reconstruction
