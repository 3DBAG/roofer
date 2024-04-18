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

#include "reconstruction/AlphaShaper.hpp"
#include "reconstruction/ArrangementBuilder.hpp"
#include "reconstruction/ArrangementDissolver.hpp"
#include "reconstruction/ArrangementExtruder.hpp"
#include "reconstruction/ArrangementOptimiser.hpp"
#include "reconstruction/ArrangementSnapper.hpp"
#include "reconstruction/LineDetector.hpp"
#include "reconstruction/LineRegulariser.hpp"
#include "reconstruction/PlaneDetector.hpp"
#include "reconstruction/PlaneIntersector.hpp"
#include "reconstruction/SegmentRasteriser.hpp"
#include "reconstruction/ElevationProvider.hpp"
#include "reconstruction/cdt_util.hpp"

#include "CGAL/Polygon_with_holes_2.h"

namespace roofer {
  /*
   * @brief Configuration parameters for single instance building reconstruction
   *
   * //todo doc
   */
    struct ReconstructionConfig {
        // control optimisation
        float lambda = 1./9.;
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
            return  (lambda>=0 && lambda <=1.0) &&
                    lod==12 || lod==13 || lod ==22 &&
                    lod13_step_height > 0;
        }
   };

  /*
   * @brief Reconstructs a single instance of a building from a point cloud with
   * one floor elevation
   *
   * //todo doc
   */
  template <typename Footprint>
  Mesh reconstruct_single_instance(const PointCollection& points_roof,
                                   const PointCollection& points_ground,
                                   Footprint& footprint,
                                   ReconstructionConfig cfg=ReconstructionConfig())
  {
    try {
      // check if configuration is valid
      if (!cfg.is_valid()) {
        throw rooferException("Invalid roofer configuration.");
      }

      // prepare footprint data type
      // template deduction will fail if not convertible to LinearRing
      if constexpr (std::is_same_v<Footprint,
                                   CGAL::Polygon_with_holes_2<EPICK>>) {
        // convert 2D footprint to LinearRing
        roofer::LinearRing linearRing;
        for (auto& p : footprint.outer_boundary()) {
          float x = p.x();
          float y = p.y();
          linearRing.push_back({x, y, 0.});
        }
        for (auto& hole : footprint.holes()) {
          vec3f iring;
          for (auto& p : hole) {
            float x = p.x();
            float y = p.y();
            iring.push_back({x, y, 0.});
          }
          linearRing.interior_rings().push_back(iring);
        }
        cfg.override_with_floor_elevation = true;
      }

      std::unique_ptr<roofer::detection::ElevationProvider> elevation_provider = nullptr;
      if (!cfg.override_with_floor_elevation) {
        proj_tri_util::DT base_cdt =
            proj_tri_util::cdt_from_linearing(footprint);
        auto base_cdt_ptr = std::make_unique<proj_tri_util::DT>(base_cdt);
        elevation_provider = roofer::detection::createElevationProvider(*base_cdt_ptr);
      } else {
        elevation_provider = roofer::detection::createElevationProvider(cfg.floor_elevation);
      }

#ifdef ROOFER_VERBOSE
      std::cout << "Reconstructing single instance" << std::endl;
      std::cout << "Running plane detectors" << std::endl;
#endif
      auto PlaneDetector = roofer::detection::createPlaneDetector();
      PlaneDetector->detect(points_roof);
      if (PlaneDetector->roof_type == "no points" ||
          PlaneDetector->roof_type == "no planes") {
        throw rooferException(
            "Pointcloud insufficient; unable to detect planes");
      }
      auto PlaneDetector_ground = roofer::detection::createPlaneDetector();
      PlaneDetector_ground->detect(points_ground);

#ifdef ROOFER_VERBOSE
      std::cout << "Computing alpha shapes" << std::endl;
#endif
      auto AlphaShaper = roofer::detection::createAlphaShaper();
      AlphaShaper->compute(PlaneDetector->pts_per_roofplane);
      if (AlphaShaper->alpha_rings.size() == 0) {
        throw rooferException(
            "Pointcloud insufficient; unable to extract boundary lines");
      }
      auto AlphaShaper_ground = roofer::detection::createAlphaShaper();
      AlphaShaper_ground->compute(PlaneDetector_ground->pts_per_roofplane);

#ifdef ROOFER_VERBOSE
      std::cout << "Running line detector" << std::endl;
#endif
      auto LineDetector = roofer::detection::createLineDetector();
      LineDetector->detect(AlphaShaper->alpha_rings, AlphaShaper->roofplane_ids,
                           PlaneDetector->pts_per_roofplane);

#ifdef ROOFER_VERBOSE
      std::cout << "Running plane intersector" << std::endl;
#endif
      auto PlaneIntersector = roofer::detection::createPlaneIntersector();
      PlaneIntersector->compute(PlaneDetector->pts_per_roofplane,
                                PlaneDetector->plane_adjacencies);

#ifdef ROOFER_VERBOSE
      std::cout << "Running line regulariser" << std::endl;
#endif
      auto LineRegulariser = roofer::detection::createLineRegulariser();
      LineRegulariser->compute(LineDetector->edge_segments,
                               PlaneIntersector->segments);

#ifdef ROOFER_VERBOSE
      std::cout << "Running segment rasteriser" << std::endl;
#endif
      auto SegmentRasteriser = roofer::detection::createSegmentRasteriser();
      auto SegmentRasterizerCfg = roofer::detection::SegmentRasteriserConfig();
      if (points_ground.empty()) {
        SegmentRasterizerCfg.use_ground = false;
        cfg.clip_ground = false;
      }
      SegmentRasteriser->compute(AlphaShaper->alpha_triangles,
                                 AlphaShaper_ground->alpha_triangles,
                                 SegmentRasterizerCfg);

#ifdef ROOFER_VERBOSE
      std::cout << "Running arrangement builder" << std::endl;
#endif
      Arrangement_2 arrangement;
      auto ArrangementBuilder = roofer::detection::createArrangementBuilder();
      ArrangementBuilder->compute(arrangement, footprint,
                                  LineRegulariser->exact_regularised_edges);

#ifdef ROOFER_VERBOSE
      std::cout << "Running arrangement optimiser" << std::endl;
#endif
      auto ArrangementOptimiser =
          roofer::detection::createArrangementOptimiser();
      ArrangementOptimiser->compute(arrangement, SegmentRasteriser->heightfield,
                                    PlaneDetector->pts_per_roofplane,
                                    PlaneDetector_ground->pts_per_roofplane,
                                    {.data_multiplier = (1 - cfg.lambda),
                                     .smoothness_multiplier = cfg.lambda,
                                     .use_ground = cfg.clip_ground});

#ifdef ROOFER_VERBOSE
      std::cout << "Running arrangement dissolver" << std::endl;
#endif
      auto ArrangementDissolver =
          roofer::detection::createArrangementDissolver();
      ArrangementDissolver->compute(
          arrangement, SegmentRasteriser->heightfield,
          {.dissolve_step_edges = cfg.lod == 13,
           .dissolve_all_interior = cfg.lod == 12,
           .step_height_threshold = cfg.lod13_step_height});

#ifdef ROOFER_VERBOSE
      std::cout << "Running arrangement snapper" << std::endl;
#endif
      auto ArrangementSnapper = roofer::detection::createArrangementSnapper();
      ArrangementSnapper->compute(arrangement);

#ifdef ROOFER_VERBOSE
      std::cout << "Running arrangement extruder" << std::endl;
#endif
      auto ArrangementExtruder = roofer::detection::createArrangementExtruder();
      ArrangementExtruder->compute(arrangement,
                                   *elevation_provider,
                                   {.LoD2 = cfg.lod == 22});

//      assert(ArrangementExtruder->meshes.size() == 1);
      //todo temp
      if (ArrangementExtruder->meshes.size() != 1) {
        throw rooferException("More than one output mesh!");
      }
      return ArrangementExtruder->meshes.front();
    } catch (const std::exception& e) {
#ifdef ROOFER_VERBOSE
      std::cout << "Reconstruction failed, exception thrown: " << e.what() << std::endl;
#endif
      throw rooferException(e.what());
    }
  }

  /*
   * @brief Reconstructs a single instance of a building from a point cloud with
   * one floor elevation
   *
   * Overload for when the ground points are not available
   * //todo doc
   */
  template <typename Footprint>
  Mesh reconstruct_single_instance(const PointCollection& points_roof,
                                   Footprint& footprint,
                                   ReconstructionConfig cfg=ReconstructionConfig())
  {
    PointCollection points_ground = PointCollection();
    return reconstruct_single_instance(points_roof, points_ground, footprint, cfg);
  }

} // namespace roofer