// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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
    float complexity_factor = 0.888;
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
    float lod13_step_height = 3.;
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
    int plane_detect_k = 15;

    /**
     * @brief Minimum number of points required for detecting a plane.
     */
    int plane_detect_min_points = 15;

    /**
     * @brief # Maximum distance from candidate points to plane during
     * plane fitting procedure. Higher values offer more robustness against
     * oversegmentation in plane detection, lower values give more planes that
     * are closer to the point cloud. Unit: meters.
     */
    float plane_detect_epsilon = 0.300000;

    /**
     * @brief Maximum allowed angle between points inside the same
     * detected plane. This value is compared to the dot product between two
     * unit normals. Eg. 0 means 90 degrees (orthogonal normals), and 1.0 means
     * 0 degrees (parallel normals)
     */
    float plane_detect_normal_angle = 0.750000;

    /**
     * @brief Maximum distance from candidate points to line during line
     * fitting procedure. Higher values offer more robustness against irregular
     * lines, lower values give more accurate lines (ie. more closely wrapping
     * around point cloud). Unit: meters.
     */
    float line_detect_epsilon = 1.000000;

    /**
     * @brief Distance used in computing alpha-shape of detected plane
     * segments prior to line detection. Higher values offer more robustness
     * against irregular lines, lower values give more accurate lines (ie. more
     * closely wrapping around point cloud). Unit: meters.
     */
    float thres_alpha = 0.250000;

    /**
     * @brief Maximum distance to merge lines during line regularisation
     * (after line detection). Approximately parallel lines that are closer to
     * each other than this threshold are merged. Higher values yield more
     * regularisation, lower values preserve more finer details. Unit: meters.
     */
    float thres_reg_line_dist = 0.800000;

    /**
     * @brief Extension of regularised lines prior to optimisation. Used
     * to compensate for undetected parts in the roofpart boundaries. Use higher
     * values when the input pointcloud is less dense. However, setting this too
     * high can lead to unrealistic reconstruction results. Unit: meters.
     */
    float thres_reg_line_ext = 3.000000;
    // lod1_extrude_to_max=false

    bool is_valid() {
      return (complexity_factor >= 0 && complexity_factor <= 1.0) &&
                 lod == 12 ||
             lod == 13 || lod == 22 && lod13_step_height > 0;
    }
  };

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
  std::vector<Mesh> reconstruct(
      const PointCollection& points_roof, const PointCollection& points_ground,
      Footprint& footprint, ReconstructionConfig cfg = ReconstructionConfig()) {
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
      PlaneDetector->detect(points_roof);
      if (PlaneDetector->roof_type == "no points" ||
          PlaneDetector->roof_type == "no planes") {
        throw rooferException(
            "Pointcloud insufficient; unable to detect planes");
      }
      auto PlaneDetector_ground = roofer::reconstruction::createPlaneDetector();
      if (!points_ground.empty()) {
        PlaneDetector_ground->detect(points_ground);
      }

      auto AlphaShaper = roofer::reconstruction::createAlphaShaper();
      AlphaShaper->compute(PlaneDetector->pts_per_roofplane);
      if (AlphaShaper->alpha_rings.size() == 0) {
        throw rooferException(
            "Pointcloud insufficient; unable to extract boundary lines");
      }
      auto AlphaShaper_ground = roofer::reconstruction::createAlphaShaper();
      AlphaShaper_ground->compute(PlaneDetector_ground->pts_per_roofplane);

      auto LineDetector = roofer::reconstruction::createLineDetector();
      LineDetector->detect(AlphaShaper->alpha_rings, AlphaShaper->roofplane_ids,
                           PlaneDetector->pts_per_roofplane);

      auto PlaneIntersector = roofer::reconstruction::createPlaneIntersector();
      PlaneIntersector->compute(PlaneDetector->pts_per_roofplane,
                                PlaneDetector->plane_adjacencies);

      auto LineRegulariser = roofer::reconstruction::createLineRegulariser();
      LineRegulariser->compute(LineDetector->edge_segments,
                               PlaneIntersector->segments);

      auto SegmentRasteriser =
          roofer::reconstruction::createSegmentRasteriser();
      auto SegmentRasterizerCfg =
          roofer::reconstruction::SegmentRasteriserConfig();
      if (points_ground.empty()) {
        SegmentRasterizerCfg.use_ground = false;
        cfg.clip_ground = false;
      }
      SegmentRasteriser->compute(AlphaShaper->alpha_triangles,
                                 AlphaShaper_ground->alpha_triangles,
                                 SegmentRasterizerCfg);

      Arrangement_2 arrangement;
      auto ArrangementBuilder =
          roofer::reconstruction::createArrangementBuilder();
      ArrangementBuilder->compute(arrangement, linear_ring,
                                  LineRegulariser->exact_regularised_edges);

      auto ArrangementOptimiser =
          roofer::reconstruction::createArrangementOptimiser();
      ArrangementOptimiser->compute(
          arrangement, SegmentRasteriser->heightfield,
          PlaneDetector->pts_per_roofplane,
          PlaneDetector_ground->pts_per_roofplane,
          {.data_multiplier = cfg.complexity_factor,
           .smoothness_multiplier = (1 - cfg.complexity_factor),
           .use_ground = cfg.clip_ground});

      auto ArrangementDissolver =
          roofer::reconstruction::createArrangementDissolver();
      ArrangementDissolver->compute(
          arrangement, SegmentRasteriser->heightfield,
          {.dissolve_step_edges = cfg.lod == 13,
           .dissolve_all_interior = cfg.lod == 12,
           .step_height_threshold = cfg.lod13_step_height});

      auto ArrangementSnapper =
          roofer::reconstruction::createArrangementSnapper();
      ArrangementSnapper->compute(arrangement);

      auto ArrangementExtruder =
          roofer::reconstruction::createArrangementExtruder();
      ArrangementExtruder->compute(arrangement, *elevation_provider,
                                   {.LoD2 = cfg.lod == 22});

      return ArrangementExtruder->meshes;

    } catch (const std::exception& e) {
#ifdef ROOFER_VERBOSE
      std::cout << "Reconstruction failed, exception thrown: " << e.what()
                << std::endl;
#endif
      throw rooferException(e.what());
    }
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
  std::vector<Mesh> reconstruct(
      const PointCollection& points_roof, Footprint& footprint,
      ReconstructionConfig cfg = ReconstructionConfig()) {
    PointCollection points_ground = PointCollection();
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
