// This file is part of gfp-building-reconstruction
// Copyright (C) 2018-2022 Ravi Peters

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
  /*
   * @brief Configuration parameters for single instance building reconstruction
   *
   * //todo doc
   */
  struct ReconstructionConfig {
    // control optimisation
    float lambda = 0.7;
    // enable clipping parts off the footprint where ground planes are detected
    bool clip_ground = true;
    // requested LoD
    int lod = 22;
    // step height used for LoD13 generalisation
    float lod13_step_height = 3.;
    // floor elevation
    float floor_elevation = 0.;
    // force flat floor
    bool override_with_floor_elevation = false;

    bool is_valid() {
      return (lambda >= 0 && lambda <= 1.0) && lod == 12 || lod == 13 ||
             lod == 22 && lod13_step_height > 0;
    }
  };

  /*
   * @brief Reconstructs a single instance of a building from a point cloud
   *
   * //todo doc
   */
  template <typename Footprint>
  std::vector<Mesh> reconstruct_single_instance(const PointCollection& points_roof,
                                                const PointCollection& points_ground,
                                                Footprint& footprint,
                                                ReconstructionConfig cfg = ReconstructionConfig()) {
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
      ArrangementOptimiser->compute(arrangement, SegmentRasteriser->heightfield,
                                    PlaneDetector->pts_per_roofplane,
                                    PlaneDetector_ground->pts_per_roofplane,
                                    {.data_multiplier = cfg.lambda,
                                     .smoothness_multiplier = (1 - cfg.lambda),
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

  /*
   * @brief Reconstructs a single instance of a building from a point cloud
   *
   * Overload for when the ground points are not available
   * //todo doc
   */
  template <typename Footprint>
  std::vector<Mesh> reconstruct_single_instance(const PointCollection& points_roof,
                                                Footprint& footprint,
                                                ReconstructionConfig cfg = ReconstructionConfig()) {
    PointCollection points_ground = PointCollection();
    return reconstruct_single_instance(points_roof,
                                       points_ground,
                                       footprint,
                                       cfg);
  }

  //todo maybe move to another location
  TriangleCollection triangulate_mesh(const Mesh& mesh) {
    auto MeshTriangulator = roofer::reconstruction::createMeshTriangulatorLegacy();
    MeshTriangulator->compute({mesh});

    return MeshTriangulator->triangles;
  }

}  // namespace roofer
