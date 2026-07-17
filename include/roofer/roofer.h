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
// Ivan Paden
// Ravi Peters
#pragma once

#include <roofer/reconstruction/AlphaShaper.hpp>
#include <roofer/reconstruction/ArrangementBuilder.hpp>
#include <roofer/reconstruction/ArrangementDissolver.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include <roofer/reconstruction/ArrangementOptimiser.hpp>
#include <roofer/reconstruction/ArrangementSnapper.hpp>
#include <roofer/reconstruction/ElevationProvider.hpp>
#include <roofer/reconstruction/LineDetector.hpp>
#include <roofer/reconstruction/LineRegulariser.hpp>
#include <roofer/reconstruction/MeshTriangulator.hpp>
#include <roofer/reconstruction/PlaneDetector.hpp>
#include <roofer/reconstruction/PlaneIntersector.hpp>
#include <roofer/reconstruction/SegmentRasteriser.hpp>
#include <roofer/reconstruction/ReconstructionConfig.hpp>
#include <roofer/reconstruction/cdt_util.hpp>

#include "CGAL/Polygon_with_holes_2.h"
namespace roofer {

  /**
   * @brief Configuration parameters for the roofer building
   * reconstruction algorithm. Coordinate units are assumed to be in meters.
   */
  struct ReconstructionConfig {
    /**
     * @brief Complexity factor for building model geometry.
     *
     * A number between `0.0` and `1.0`. Higher values lead to
     * more detailed building models, lower values to simpler models.
     */
    float complexity_factor =
        reconstruction::ArrangementOptimiserConfig{}.complexity_factor;
    /**
     * @brief Set to true to activate the procedure that
     * clips parts from the input footprint wherever patches of ground points
     * are detected. May cause irregular outlines in reconstruction result.
     */
    bool clip_ground = true;
    /**
     * @brief Requested Level of Detail
     * - 12: LoD 1.2
     * - 13: LoD 1.3
     * - 22: LoD 2.2
     */
    int lod = 22;
    /**
     * @brief Step height used for LoD13 generalisation, i.e. roofparts with a
     * height discontinuity that is smaller than this value are merged. Only
     * affects LoD 1.3 reconstructions. Unit: meters.
     */
    float lod13_step_height =
        reconstruction::ReconstructionConfig{}.lod13_step_height;
    /**
     * @brief Floor elevation in case it is not provided by the
     * footprint (API only).
     */
    float floor_elevation = 0.;
    /**
     * @brief Force flat floor instead of using the
     * elevation of the footprint (API only).
     */
    bool override_with_floor_elevation = false;

    /**
     * @brief Number of points used in nearest neighbour queries
     * during plane detection.
     */
    int plane_detect_k =
        reconstruction::PlaneDetectorConfig{}.plane_neighbour_count;

    /**
     * @brief Minimum number of points required for detecting a plane.
     */
    int plane_detect_min_points =
        reconstruction::PlaneDetectorConfig{}.min_plane_points;

    /**
     * @brief # Maximum distance from candidate points to plane during
     * plane fitting procedure. Higher values offer more robustness against
     * oversegmentation in plane detection, lower values give more planes that
     * are closer to the point cloud. Unit: meters.
     */
    float plane_detect_epsilon =
        reconstruction::PlaneDetectorConfig{}.plane_epsilon;

    /**
     * @brief Maximum allowed angle between points inside the same
     * detected plane. This value is compared to the dot product between two
     * unit normals. Eg. 0 means 90 degrees (orthogonal normals), and 1.0 means
     * 0 degrees (parallel normals)
     */
    float plane_detect_normal_angle =
        reconstruction::PlaneDetectorConfig{}.plane_normal_threshold;

    /**
     * @brief Maximum distance from candidate points to line during line
     * fitting procedure. Higher values offer more robustness against irregular
     * lines, lower values give more accurate lines (ie. more closely wrapping
     * around point cloud). Unit: meters.
     */
    float line_detect_epsilon =
        reconstruction::LineDetectorConfig{}.distance_threshold;

    /**
     * @brief Squared-radius parameter used in computing the alpha-shape of
     * detected plane segments prior to line detection. Higher values offer
     * more robustness against irregular lines, lower values give more accurate
     * lines (ie. more closely wrapping around point cloud). Unit: square
     * metres.
     */
    float thres_alpha = reconstruction::AlphaShaperConfig{}.alpha;

    /**
     * @brief Maximum distance to merge lines during line regularisation
     * (after line detection). Approximately parallel lines that are closer to
     * each other than this threshold are merged. Higher values yield more
     * regularisation, lower values preserve more finer details. Unit: meters.
     */
    float thres_reg_line_dist =
        reconstruction::LineRegulariserConfig{}.distance_threshold;

    /**
     * @brief Extension of regularised lines prior to optimisation. Used
     * to compensate for undetected parts in the roofpart boundaries. Use higher
     * values when the input pointcloud is less dense. However, setting this too
     * high can lead to unrealistic reconstruction results. Unit: meters.
     */
    float thres_reg_line_ext =
        reconstruction::LineRegulariserConfig{}.extension;
    // lod1_extrude_to_max=false

    [[nodiscard]] bool is_valid() const;
  };

  /** Options for the nested reconstruction API. */
  struct ReconstructOptions {
    ReconstructOptions() = default;
    reconstruction::ReconstructionConfig reconstruction;
    int lod = 22;
    float floor_elevation = 0.0F;
    bool override_with_floor_elevation = false;

    [[nodiscard]] bool is_valid() const {
      return (lod == 12 || lod == 13 || lod == 22) &&
             !reconstruction.validate().has_value();
    }
  };

  inline ReconstructOptions to_reconstruct_options(
      const ReconstructionConfig& legacy) {
    ReconstructOptions options;
    options.lod = legacy.lod;
    options.floor_elevation = legacy.floor_elevation;
    options.override_with_floor_elevation =
        legacy.override_with_floor_elevation;
    options.reconstruction.clip_terrain = legacy.clip_ground;
    options.reconstruction.lod13_step_height = legacy.lod13_step_height;
    options.reconstruction.arrangement_optimiser.complexity_factor =
        legacy.complexity_factor;
    options.reconstruction.plane_detector.plane_neighbour_count =
        legacy.plane_detect_k;
    options.reconstruction.plane_detector.min_plane_points =
        legacy.plane_detect_min_points;
    options.reconstruction.plane_detector.plane_epsilon =
        legacy.plane_detect_epsilon;
    options.reconstruction.plane_detector.plane_normal_threshold =
        legacy.plane_detect_normal_angle;
    options.reconstruction.line_detector.distance_threshold =
        legacy.line_detect_epsilon;
    options.reconstruction.alpha_shaper.alpha = legacy.thres_alpha;
    options.reconstruction.line_regulariser.distance_threshold =
        legacy.thres_reg_line_dist;
    options.reconstruction.line_regulariser.extension =
        legacy.thres_reg_line_ext;
    return options;
  }

  inline bool ReconstructionConfig::is_valid() const {
    return to_reconstruct_options(*this).is_valid();
  }

  /**
   * @brief Reconstructs a single instance of a building from a point cloud
   *
   * @tparam Footprint Type of the footprint, either a `LinearRing` or a
   * `CGAL::Polygon_with_holes_2<Epick>`
   *
   * @param points_roof Point cloud representing the roof points
   * @param points_ground Point cloud representing the ground points
   * @param footprint Footprint of the building
   * @param cfg Configuration parameters
   *
   * @return std::vector<Mesh> Building geometry meshes. The number of meshes is
   * equal to the number of building parts.
   */
  template <typename Footprint>
  std::vector<Mesh> reconstruct(const PointCollection& points_roof,
                                const PointCollection& points_ground,
                                Footprint& footprint, ReconstructOptions cfg) {
    try {
      // check if configuration is valid
      if (!cfg.is_valid()) {
        throw rooferException("Invalid roofer configuration.");
      }

      // prepare footprint data type
      // template deduction will fail if not convertible to LinearRing
      roofer::LinearRing linear_ring;
      if constexpr (std::is_same_v<Footprint,
                                   CGAL::Polygon_with_holes_2<EPICK>>) {
        // convert 2D footprint to LinearRing
        for (auto& p : footprint.outer_boundary()) {
          float x = p.x();
          float y = p.y();
          linear_ring.push_back({x, y, 0.});
        }
        for (auto& hole : footprint.holes()) {
          vec3f iring;
          for (auto& p : hole) {
            float x = p.x();
            float y = p.y();
            iring.push_back({x, y, 0.});
          }
          linear_ring.interior_rings().push_back(iring);
        }
        cfg.override_with_floor_elevation = true;
      } else {
        // Footprint is already a LinearRing
        linear_ring = footprint;
      }
      pop_back_if_equal_to_front(linear_ring);

      std::unique_ptr<roofer::reconstruction::ElevationProvider>
          elevation_provider = nullptr;
      if (!cfg.override_with_floor_elevation) {
        proj_tri_util::DT base_cdt =
            proj_tri_util::cdt_from_linearing(linear_ring);
        auto base_cdt_ptr = std::make_unique<proj_tri_util::DT>(base_cdt);
        elevation_provider =
            roofer::reconstruction::createElevationProvider(*base_cdt_ptr);
      } else {
        elevation_provider = roofer::reconstruction::createElevationProvider(
            cfg.floor_elevation);
      }

      auto PlaneDetector = roofer::reconstruction::createPlaneDetector();
      PlaneDetector->detect(points_roof, cfg.reconstruction.plane_detector);
      if (PlaneDetector->roof_type == "no points" ||
          PlaneDetector->roof_type == "no planes") {
        throw rooferException(
            "Pointcloud insufficient; unable to detect planes");
      }
      auto PlaneDetector_ground = roofer::reconstruction::createPlaneDetector();
      if (!points_ground.empty()) {
        PlaneDetector_ground->detect(points_ground,
                                     cfg.reconstruction.plane_detector);
      }

      auto AlphaShaper = roofer::reconstruction::createAlphaShaper();
      AlphaShaper->compute(PlaneDetector->pts_per_roofplane,
                           cfg.reconstruction.alpha_shaper);
      if (AlphaShaper->alpha_rings.size() == 0) {
        throw rooferException(
            "Pointcloud insufficient; unable to extract boundary lines");
      }
      auto AlphaShaper_ground = roofer::reconstruction::createAlphaShaper();
      AlphaShaper_ground->compute(PlaneDetector_ground->pts_per_roofplane,
                                  cfg.reconstruction.alpha_shaper);

      auto LineDetector = roofer::reconstruction::createLineDetector();
      LineDetector->detect(AlphaShaper->alpha_rings, AlphaShaper->roofplane_ids,
                           PlaneDetector->pts_per_roofplane,
                           cfg.reconstruction.line_detector);

      auto PlaneIntersector = roofer::reconstruction::createPlaneIntersector();
      PlaneIntersector->compute(PlaneDetector->pts_per_roofplane,
                                PlaneDetector->plane_adjacencies,
                                cfg.reconstruction.plane_intersector);

      auto LineRegulariser = roofer::reconstruction::createLineRegulariser();
      LineRegulariser->compute(LineDetector->edge_segments,
                               PlaneIntersector->segments,
                               cfg.reconstruction.line_regulariser);

      auto SegmentRasteriser =
          roofer::reconstruction::createSegmentRasteriser();
      auto SegmentRasterizerCfg = cfg.reconstruction.segment_rasteriser;
      if (points_ground.empty()) {
        SegmentRasterizerCfg.use_ground = false;
        cfg.reconstruction.clip_terrain = false;
      }
      SegmentRasteriser->compute(AlphaShaper->alpha_triangles,
                                 AlphaShaper_ground->alpha_triangles,
                                 SegmentRasterizerCfg);

      Arrangement_2 arrangement;
      auto ArrangementBuilder =
          roofer::reconstruction::createArrangementBuilder();
      ArrangementBuilder->compute(arrangement, linear_ring,
                                  LineRegulariser->exact_regularised_edges,
                                  cfg.reconstruction.arrangement_builder);

      auto ArrangementOptimiser =
          roofer::reconstruction::createArrangementOptimiser();
      auto optimiser_config = cfg.reconstruction.arrangement_optimiser;
      optimiser_config.use_ground = cfg.reconstruction.clip_terrain;
      ArrangementOptimiser->compute(arrangement, SegmentRasteriser->heightfield,
                                    PlaneDetector->pts_per_roofplane,
                                    PlaneDetector_ground->pts_per_roofplane,
                                    optimiser_config);

      auto ArrangementDissolver =
          roofer::reconstruction::createArrangementDissolver();
      auto dissolver_config = cfg.reconstruction.arrangement_dissolver;
      dissolver_config.dissolve_step_edges = cfg.lod == 13;
      dissolver_config.dissolve_all_interior = cfg.lod == 12;
      dissolver_config.step_height_threshold =
          cfg.reconstruction.lod13_step_height;
      ArrangementDissolver->compute(arrangement, SegmentRasteriser->heightfield,
                                    *elevation_provider, dissolver_config);

      auto ArrangementSnapper =
          roofer::reconstruction::createArrangementSnapper();
      ArrangementSnapper->compute(arrangement, *elevation_provider,
                                  cfg.reconstruction.arrangement_snapper);

      auto ArrangementExtruder =
          roofer::reconstruction::createArrangementExtruder();
      auto extruder_config = cfg.reconstruction.arrangement_extruder;
      extruder_config.lod2 = cfg.lod == 22;
      ArrangementExtruder->compute(arrangement, *elevation_provider,
                                   extruder_config);

      return ArrangementExtruder->meshes;

    } catch (const std::exception& e) {
#ifdef ROOFER_VERBOSE
      std::cout << "Reconstruction failed, exception thrown: " << e.what()
                << std::endl;
#endif
      throw rooferException(e.what());
    }
  }

  /** Compatibility overload for the flattened 1.x configuration. */
  template <typename Footprint>
  [[deprecated("Use reconstruct(..., ReconstructOptions) instead")]]
  std::vector<Mesh> reconstruct(
      const PointCollection& points_roof, const PointCollection& points_ground,
      Footprint& footprint, ReconstructionConfig cfg = ReconstructionConfig()) {
    return reconstruct(points_roof, points_ground, footprint,
                       to_reconstruct_options(cfg));
  }

  /**
   * @brief Reconstructs a single instance of a building from a point cloud
   * Overload for when the ground points are not available
   *
   * @tparam Footprint Type of the footprint, either a `LinearRing` or a
   * `CGAL::Polygon_with_holes_2<Epick>`
   *
   * @param points_roof Point cloud representing the roof points
   * @param footprint Footprint of the building
   * @param cfg Configuration parameters
   *
   * @return std::vector<Mesh> Building geometry meshes. The number of meshes is
   * equal to the number of building parts. This number is equal to one if no
   * ground points are available.
   */
  template <typename Footprint>
  [[deprecated("Use reconstruct(..., ReconstructOptions) instead")]]
  std::vector<Mesh> reconstruct(
      const PointCollection& points_roof, Footprint& footprint,
      ReconstructionConfig cfg = ReconstructionConfig()) {
    PointCollection points_ground = PointCollection();
    return reconstruct(points_roof, points_ground, footprint,
                       to_reconstruct_options(cfg));
  }

  template <typename Footprint>
  std::vector<Mesh> reconstruct(const PointCollection& points_roof,
                                Footprint& footprint, ReconstructOptions cfg) {
    PointCollection points_ground;
    return reconstruct(points_roof, points_ground, footprint, cfg);
  }

  /**
   * @brief Triangulates a mesh using
   * `roofer::reconstruction::MeshTriangulatorLegacy`
   *
   * @param mesh Mesh to triangulate
   *
   * @return TriangleCollection Triangulated mesh
   */
  TriangleCollection triangulate_mesh(const Mesh& mesh) {
    auto MeshTriangulator =
        roofer::reconstruction::createMeshTriangulatorLegacy();
    MeshTriangulator->compute({mesh});

    return MeshTriangulator->triangles;
  }

}  // namespace roofer
