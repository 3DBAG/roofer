// Copyright (c) 2018-2025 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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
#ifdef RF_USE_RERUN
// Adapters so we can log eigen vectors as rerun positions:
template <>
struct rerun::CollectionAdapter<rerun::Position3D, roofer::PointCollection> {
  /// Borrow for non-temporary.
  Collection<rerun::Position3D> operator()(
      const roofer::PointCollection& container) {
    return Collection<rerun::Position3D>::borrow(container.data(),
                                                 container.size());
  }

  // Do a full copy for temporaries (otherwise the data might be deleted when
  // the temporary is destroyed).
  Collection<rerun::Position3D> operator()(
      roofer::PointCollection&& container) {
    std::vector<rerun::Position3D> positions(container.size());
    memcpy(positions.data(), container.data(),
           container.size() * sizeof(roofer::arr3f));
    return Collection<rerun::Position3D>::take_ownership(std::move(positions));
  }
};
template <>
struct rerun::CollectionAdapter<rerun::Position3D, roofer::TriangleCollection> {
  /// Borrow for non-temporary.
  Collection<rerun::Position3D> operator()(
      const roofer::TriangleCollection& container) {
    return Collection<rerun::Position3D>::borrow(container[0].data(),
                                                 container.vertex_count());
  }

  // Do a full copy for temporaries (otherwise the data might be deleted when
  // the temporary is destroyed).
  Collection<rerun::Position3D> operator()(
      roofer::TriangleCollection&& container) {
    std::vector<rerun::Position3D> positions(container.size());
    memcpy(positions.data(), container[0].data(),
           container.vertex_count() * sizeof(roofer::arr3f));
    return Collection<rerun::Position3D>::take_ownership(std::move(positions));
  }
};
#endif

enum LOD { LOD11 = 11, LOD12 = 12, LOD13 = 13, LOD22 = 22 };

void add_ms_to_bbox(roofer::Box& box,
                    std::unordered_map<int, roofer::Mesh>& multisolid) {
  for (auto& [i, mesh] : multisolid) {
    auto& faces = mesh.get_polygons();
    auto& labels = mesh.get_labels();
    assert(faces.size() == labels.size());
    size_t n_faces = faces.size();

    auto& attributes = mesh.get_attributes();
    assert(attributes.size() == n_faces);
    for (size_t i = 0; i < n_faces; ++i) {
      // 1 is the roof label
      if (labels[i] == 1) {
        box.add(faces[i].box());
      }
    }
  }
}

void compute_mesh_properties(
    std::unordered_map<int, roofer::Mesh>& multisolid_lod12,
    std::unordered_map<int, roofer::Mesh>& multisolid_lod13,
    std::unordered_map<int, roofer::Mesh>& multisolid_lod22, float z_offset,
    RooferConfig* cfg) {
  auto MeshPropertyCalculator = roofer::misc::createMeshPropertyCalculator();

  // make sure bbox is initialized properly
  roofer::Box bbox;
  if (multisolid_lod12.size()) {
    add_ms_to_bbox(bbox, multisolid_lod12);
  }
  if (multisolid_lod13.size()) {
    add_ms_to_bbox(bbox, multisolid_lod13);
  }
  if (multisolid_lod22.size()) {
    add_ms_to_bbox(bbox, multisolid_lod22);
  }

  auto heightmap = MeshPropertyCalculator->get_heightmap(multisolid_lod22, bbox,
                                                         cfg->cellsize);

  for (auto& [i, mesh22] : multisolid_lod22) {
    mesh22.get_attributes().resize(mesh22.get_polygons().size());

    MeshPropertyCalculator->calculate_h_attr(mesh22, heightmap,
                                             {.z_offset = z_offset,
                                              .h_50p = cfg->a_h_roof_50p,
                                              .h_70p = cfg->a_h_roof_70p,
                                              .h_min = cfg->a_h_roof_min,
                                              .h_max = cfg->a_h_roof_max});

    MeshPropertyCalculator->compute_roof_orientation(
        mesh22, {.slope = cfg->a_slope, .azimuth = cfg->a_azimuth});

    if (multisolid_lod12.size() > i) {
      auto& mesh12 = multisolid_lod12.at(i);
      mesh12.get_attributes().resize(mesh12.get_polygons().size());
      MeshPropertyCalculator->calculate_h_attr(mesh12, heightmap,
                                               {.z_offset = z_offset,
                                                .h_50p = cfg->a_h_roof_50p,
                                                .h_70p = cfg->a_h_roof_70p,
                                                .h_min = cfg->a_h_roof_min,
                                                .h_max = cfg->a_h_roof_max});
    }
    if (multisolid_lod13.size() > i) {
      auto& mesh13 = multisolid_lod13.at(i);
      mesh13.get_attributes().resize(mesh13.get_polygons().size());
      MeshPropertyCalculator->calculate_h_attr(mesh13, heightmap,
                                               {.z_offset = z_offset,
                                                .h_50p = cfg->a_h_roof_50p,
                                                .h_70p = cfg->a_h_roof_70p,
                                                .h_min = cfg->a_h_roof_min,
                                                .h_max = cfg->a_h_roof_max});
    }
  }
}

void multisolid_post_process(BuildingObject& building, RooferConfig* cfg,
                             LOD lod,
                             std::unordered_map<int, roofer::Mesh>& multisolid,
                             std::optional<float>& rmse,
                             std::optional<float>& volume,
                             std::optional<std::string>& attr_val3dity) {
  auto MeshTriangulator =
      roofer::reconstruction::createMeshTriangulatorLegacy();
  MeshTriangulator->compute(multisolid);
  volume = MeshTriangulator->volumes.at(0);
  // logger.debug("Completed MeshTriangulator");
#ifdef RF_USE_RERUN
  if (cfg->use_rerun) {
    const auto& rec = rerun::RecordingStream::current();
    std::string worldname = fmt::format("world/lod{}/", (int)lod);
    rec.log(worldname + "MeshTriangulator",
            rerun::Mesh3D(MeshTriangulator->triangles)
                .with_vertex_normals(MeshTriangulator->normals)
                .with_class_ids(MeshTriangulator->ring_ids));
  }
#endif

  auto PC2MeshDistCalculator = roofer::misc::createPC2MeshDistCalculator();
  PC2MeshDistCalculator->compute(building.pointcloud_building,
                                 MeshTriangulator->multitrianglecol,
                                 MeshTriangulator->ring_ids);
  rmse = PC2MeshDistCalculator->rms_error;
  // logger.debug("Completed PC2MeshDistCalculator. RMSE={}",
  //  PC2MeshDistCalculator->rms_error);
#ifdef RF_USE_RERUN
// rec.log(worldname+"PC2MeshDistCalculator",
// rerun::Mesh3D(PC2MeshDistCalculator->triangles).with_vertex_normals(MeshTriangulator->normals).with_class_ids(MeshTriangulator->ring_ids));
#endif

#ifdef RF_USE_VAL3DITY
  if (multisolid.size() > 0) {
    auto Val3dator = roofer::misc::createVal3dator();
    Val3dator->compute(multisolid);
    attr_val3dity = Val3dator->errors.front();
  }
  // logger.debug("Completed Val3dator. Errors={}", Val3dator->errors.front());
#endif
}

std::unordered_map<int, roofer::Mesh> extrude_lod22(
    roofer::Arrangement_2 arrangement, BuildingObject& building,
    RooferConfig* cfg,
    roofer::reconstruction::SegmentRasteriserInterface* SegmentRasteriser,
    LOD lod, std::optional<float>& rmse, std::optional<float>& volume,
    std::optional<std::string>& attr_val3dity) {
  bool dissolve_step_edges = false;
  bool dissolve_all_interior = false;
  bool extrude_LoD2 = true;
  if (lod == LOD12) {
    dissolve_all_interior = true, extrude_LoD2 = false;
  } else if (lod == LOD13) {
    dissolve_step_edges = true, extrude_LoD2 = false;
  }
#ifdef RF_USE_RERUN
  const auto& rec = rerun::RecordingStream::current();
  std::string worldname = fmt::format("world/lod{}/", (int)lod);
#endif

  auto& logger = roofer::logger::Logger::get_logger();

  auto ArrangementDissolver =
      roofer::reconstruction::createArrangementDissolver();
  ArrangementDissolver->compute(
      arrangement, SegmentRasteriser->heightfield,
      {.dissolve_step_edges = dissolve_step_edges,
       .dissolve_all_interior = dissolve_all_interior,
       .step_height_threshold = cfg->lod13_step_height});
  // logger.debug("Completed ArrangementDissolver");
  // logger.debug("Roof partition has {} faces", arrangement.number_of_faces());
#ifdef RF_USE_RERUN
  if (cfg->use_rerun) {
    rec.log(
        worldname + "ArrangementDissolver",
        rerun::LineStrips3D(roofer::reconstruction::arr2polygons(arrangement)));
  }
#endif
  auto ArrangementSnapper = roofer::reconstruction::createArrangementSnapper();
  ArrangementSnapper->compute(arrangement);
  // logger.debug("Completed ArrangementSnapper");
#ifdef RF_USE_RERUN
// rec.log(worldname+"ArrangementSnapper", rerun::LineStrips3D(
// roofer::reconstruction::arr2polygons(arrangement) ));
#endif

  auto ArrangementExtruder =
      roofer::reconstruction::createArrangementExtruder();
  ArrangementExtruder->compute(arrangement, building.h_ground,
                               {.LoD2 = extrude_LoD2});
  // logger.debug("Completed ArrangementExtruder");
#ifdef RF_USE_RERUN
  if (cfg->use_rerun) {
    rec.log(worldname + "ArrangementExtruder",
            rerun::LineStrips3D(ArrangementExtruder->faces)
                .with_class_ids(ArrangementExtruder->labels));
  }
#endif

  multisolid_post_process(building, cfg, lod, ArrangementExtruder->multisolid,
                          rmse, volume, attr_val3dity);

  return ArrangementExtruder->multisolid;
}

void extrude_lod11(BuildingObject& building, float extrusion_h,
                   RooferConfig* cfg) {
  auto SimplePolygonExtruder =
      roofer::reconstruction::createSimplePolygonExtruder();
  SimplePolygonExtruder->compute(building.footprint, building.h_ground,
                                 extrusion_h);
  // std::vector<std::unordered_map<int, roofer::Mesh>> multisolidvec;
  building.multisolids_lod12 = SimplePolygonExtruder->multisolid;
  building.multisolids_lod13 = SimplePolygonExtruder->multisolid;
  building.multisolids_lod22 = SimplePolygonExtruder->multisolid;

  compute_mesh_properties(building.multisolids_lod12,
                          building.multisolids_lod13,
                          building.multisolids_lod22, building.z_offset, cfg);

  multisolid_post_process(
      building, cfg, LOD11, SimplePolygonExtruder->multisolid,
      building.rmse_lod12, building.volume_lod12, building.val3dity_lod12);
  building.rmse_lod13 = building.rmse_lod12;
  building.rmse_lod22 = building.rmse_lod12;
  building.volume_lod13 = building.volume_lod12;
  building.volume_lod22 = building.volume_lod12;
#ifdef RF_USE_VAL3DITY
  building.val3dity_lod13 = building.val3dity_lod12;
  building.val3dity_lod22 = building.val3dity_lod12;
#endif

  building.extrusion_mode = LOD11_FALLBACK;
  building.roof_elevation_70p = building.h_pc_roof_70p + building.z_offset;
}

void reconstruct_building(BuildingObject& building, RooferConfig* cfg) {
  auto& logger = roofer::logger::Logger::get_logger();

#ifdef RF_USE_RERUN
  // const auto& rec = rerun::RecordingStream::current();
  const auto rec = rerun::RecordingStream(building.jsonl_path.string());
  // rec.save(building.jsonl_path.string() + ".rrd");
  if (cfg->use_rerun) {
    rec.spawn().exit_on_failure();
    rec.set_thread_local();
  }
  // roofer::vec3f testpts;
  // testpts.push_back({1.0f, 2.0f, 3.0f});
  // rec.log("test", rerun::Points3D(testpts));

#endif

#ifdef RF_USE_RERUN
  // rec.log("world/raw_points",
  //         rerun::AnnotationContext({
  //             rerun::AnnotationInfo(6, "BUILDING", rerun::Rgba32(255, 0, 0)),
  //             rerun::AnnotationInfo(2, "GROUND"),
  //             rerun::AnnotationInfo(1, "UNCLASSIFIED"),
  //         }));
  if (cfg->use_rerun) {
    rec.log("world/building_points",
            rerun::Points3D(building.pointcloud_building));
    rec.log("world/ground_points", rerun::Points3D(building.pointcloud_ground));
  }
#endif

  std::unordered_map<std::string, std::chrono::duration<double>> timings;

  // pointcloud_insufficient is set by StreamCropper to indicate if the
  // pointcloud has a pt_density that is lower than 2 std deviations less
  // than the mean pt_density of the tile
  if (building.pointcloud_insufficient && cfg->clear_if_insufficient) {
    building.extrusion_mode = SKIP;
  }

  if (building.extrusion_mode == SKIP) {
    if (building.roof_h_fallback.has_value()) {
      extrude_lod11(building, *building.roof_h_fallback, cfg);
    }
    return;
  } else if (building.extrusion_mode == LOD11_FALLBACK) {
    extrude_lod11(building, building.h_pc_roof_70p, cfg);
    return;
  } else if (building.extrusion_mode == STANDARD) {
    auto t0 = std::chrono::high_resolution_clock::now();
    auto PlaneDetector = roofer::reconstruction::createPlaneDetector();
    auto PlaneDetector_ground = roofer::reconstruction::createPlaneDetector();
    try {
      auto plane_detector_cfg = roofer::reconstruction::PlaneDetectorConfig{
          .metrics_plane_k = cfg->plane_detect_k,
          .metrics_plane_min_points = cfg->plane_detect_min_points,
          .metrics_plane_epsilon = cfg->plane_detect_epsilon,
          .metrics_plane_normal_threshold = cfg->plane_detect_normal_angle,
          .with_limits = true,
          .limit_n_regions = cfg->lod11_fallback_planes,
          .limit_n_milliseconds = cfg->lod11_fallback_time,
      };
      PlaneDetector->detect(building.pointcloud_building, plane_detector_cfg);
      timings["PlaneDetector"] = std::chrono::high_resolution_clock::now() - t0;
      t0 = std::chrono::high_resolution_clock::now();
      PlaneDetector_ground->detect(building.pointcloud_ground,
                                   plane_detector_cfg);
      timings["PlaneDetector_ground"] =
          std::chrono::high_resolution_clock::now() - t0;

      building.roof_type = PlaneDetector->roof_type;
      building.roof_elevation_50p =
          PlaneDetector->roof_elevation_50p + building.z_offset;
      building.roof_elevation_70p =
          PlaneDetector->roof_elevation_70p + building.z_offset;
      building.roof_elevation_min =
          PlaneDetector->roof_elevation_min + building.z_offset;
      building.roof_elevation_max =
          PlaneDetector->roof_elevation_max + building.z_offset;
      building.roof_n_planes = PlaneDetector->pts_per_roofplane.size();

      bool pointcloud_insufficient = PlaneDetector->roof_type == "no points" ||
                                     PlaneDetector->roof_type == "no planes";
      if (pointcloud_insufficient) {
        building.extrusion_mode = SKIP;
        building.pointcloud_insufficient = true;
        if (building.roof_h_fallback.has_value()) {
          extrude_lod11(building, *building.roof_h_fallback, cfg);
        }
        return;
      }
    } catch (const std::runtime_error& e) {
      extrude_lod11(building, building.h_pc_roof_70p, cfg);
      logger.warning("[reconstructor] {}, LoD1.1 fallback: {}",
                     building.jsonl_path.string(), e.what());
      return;
    }
    // #ifdef RF_USE_RERUN
    //     rec.log("world/segmented_points",
    //             rerun::Collection{rerun::components::AnnotationContext{
    //                 rerun::datatypes::AnnotationInfo(
    //                     0, "no plane", rerun::datatypes::Rgba32(30, 30,
    //                     30))}});
    //     rec.log(
    //         "world/segmented_points",
    //         rerun::Points3D(points_roof).with_class_ids(PlaneDetector->plane_id));
    // #endif
    t0 = std::chrono::high_resolution_clock::now();
    auto AlphaShaper = roofer::reconstruction::createAlphaShaper();
    AlphaShaper->compute(PlaneDetector->pts_per_roofplane,
                         {.thres_alpha = cfg->thres_alpha});
    timings["AlphaShaper"] = std::chrono::high_resolution_clock::now() - t0;
    // logger.debug("Completed AlphaShaper (roof), found {} rings, {} labels",
    //  AlphaShaper->alpha_rings.size(),
    //  AlphaShaper->roofplane_ids.size());
#ifdef RF_USE_RERUN
    if (cfg->use_rerun) {
      rec.log("world/alpha_rings_roof",
              rerun::LineStrips3D(AlphaShaper->alpha_rings)
                  .with_class_ids(AlphaShaper->roofplane_ids));
    }
#endif
    t0 = std::chrono::high_resolution_clock::now();
    auto AlphaShaper_ground = roofer::reconstruction::createAlphaShaper();
    AlphaShaper_ground->compute(PlaneDetector_ground->pts_per_roofplane);
    timings["AlphaShaper_ground"] =
        std::chrono::high_resolution_clock::now() - t0;
    // logger.debug("Completed AlphaShaper (ground), found {} rings, {} labels",
    //  AlphaShaper_ground->alpha_rings.size(),
    //  AlphaShaper_ground->roofplane_ids.size());
#ifdef RF_USE_RERUN
    if (cfg->use_rerun) {
      rec.log("world/alpha_rings_ground",
              rerun::LineStrips3D(AlphaShaper_ground->alpha_rings)
                  .with_class_ids(AlphaShaper_ground->roofplane_ids));
    }
#endif
    t0 = std::chrono::high_resolution_clock::now();
    auto LineDetector = roofer::reconstruction::createLineDetector();
    LineDetector->detect(AlphaShaper->alpha_rings, AlphaShaper->roofplane_ids,
                         PlaneDetector->pts_per_roofplane,
                         {.dist_thres = cfg->line_detect_epsilon});
    timings["LineDetector"] = std::chrono::high_resolution_clock::now() - t0;
    // logger.debug("Completed LineDetector");
#ifdef RF_USE_RERUN
    if (cfg->use_rerun) {
      rec.log("world/boundary_lines",
              rerun::LineStrips3D(LineDetector->edge_segments));
    }
#endif

    t0 = std::chrono::high_resolution_clock::now();
    auto PlaneIntersector = roofer::reconstruction::createPlaneIntersector();
    PlaneIntersector->compute(PlaneDetector->pts_per_roofplane,
                              PlaneDetector->plane_adjacencies);
    timings["PlaneIntersector"] =
        std::chrono::high_resolution_clock::now() - t0;

    size_t hr_i;
    float hr_z;
    building.roof_n_ridgelines =
        PlaneIntersector->find_highest_ridgeline(hr_z, hr_i);
    if (building.roof_n_ridgelines > 0) {
      building.roof_elevation_ridge = hr_z;
    }
    // logger.debug("Completed PlaneIntersector");
#ifdef RF_USE_RERUN
    if (cfg->use_rerun) {
      rec.log("world/intersection_lines",
              rerun::LineStrips3D(PlaneIntersector->segments));
    }
#endif

    t0 = std::chrono::high_resolution_clock::now();
    auto LineRegulariser = roofer::reconstruction::createLineRegulariser();
    LineRegulariser->compute(LineDetector->edge_segments,
                             PlaneIntersector->segments,
                             {.dist_threshold = cfg->thres_reg_line_dist,
                              .extension = cfg->thres_reg_line_ext});
    timings["LineRegulariser"] = std::chrono::high_resolution_clock::now() - t0;
    // logger.debug("Completed LineRegulariser");
#ifdef RF_USE_RERUN
    if (cfg->use_rerun) {
      rec.log("world/regularised_lines",
              rerun::LineStrips3D(LineRegulariser->regularised_edges));
    }
#endif

    t0 = std::chrono::high_resolution_clock::now();
    auto SegmentRasteriser = roofer::reconstruction::createSegmentRasteriser();
    SegmentRasteriser->compute(
        AlphaShaper->alpha_triangles, AlphaShaper_ground->alpha_triangles,
        {.use_ground =
             !building.pointcloud_ground.empty() && cfg->clip_ground});
    timings["SegmentRasteriser"] =
        std::chrono::high_resolution_clock::now() - t0;
    // logger.debug("Completed SegmentRasteriser");

#ifdef RF_USE_RERUN
    if (cfg->use_rerun) {
      auto heightfield_copy = SegmentRasteriser->heightfield;
      heightfield_copy.set_nodata(0);
      rec.log(
          "world/heightfield",
          rerun::DepthImage(heightfield_copy.vals_->data(),
                            {static_cast<uint32_t>(heightfield_copy.dimx_),
                             static_cast<uint32_t>(heightfield_copy.dimy_)}));
    }
#endif

    t0 = std::chrono::high_resolution_clock::now();
    roofer::Arrangement_2 arrangement;
    auto ArrangementBuilder =
        roofer::reconstruction::createArrangementBuilder();
    ArrangementBuilder->compute(arrangement, building.footprint,
                                LineRegulariser->exact_regularised_edges);
    timings["ArrangementBuilder"] =
        std::chrono::high_resolution_clock::now() - t0;
    // logger.debug("Completed ArrangementBuilder");
    // logger.debug("Roof partition has {} faces",
    // arrangement.number_of_faces());
#ifdef RF_USE_RERUN
    if (cfg->use_rerun) {
      rec.log("world/initial_partition",
              rerun::LineStrips3D(
                  roofer::reconstruction::arr2polygons(arrangement)));
    }
#endif

    t0 = std::chrono::high_resolution_clock::now();
    auto ArrangementOptimiser =
        roofer::reconstruction::createArrangementOptimiser();
    ArrangementOptimiser->compute(
        arrangement, SegmentRasteriser->heightfield,
        PlaneDetector->pts_per_roofplane,
        PlaneDetector_ground->pts_per_roofplane,
        {
            .data_multiplier = cfg->complexity_factor,
            .smoothness_multiplier = float(1. - cfg->complexity_factor),
            .use_ground =
                !building.pointcloud_ground.empty() && cfg->clip_ground,
        });
    timings["ArrangementOptimiser"] =
        std::chrono::high_resolution_clock::now() - t0;
    // logger.debug("Completed ArrangementOptimiser");
    // rec.log("world/optimised_partition", rerun::LineStrips3D(
    // roofer::reconstruction::arr2polygons(arrangement) ));

    // LoDs
    // attributes to be filled during reconstruction
    // logger.debug("LoD={}", cfg->lod);
    t0 = std::chrono::high_resolution_clock::now();
    if (cfg->lod_12) {
      building.multisolids_lod12 = extrude_lod22(
          arrangement, building, cfg, SegmentRasteriser.get(), LOD12,
          building.rmse_lod12, building.volume_lod12, building.val3dity_lod12);
    }

    if (cfg->lod_13) {
      building.multisolids_lod13 = extrude_lod22(
          arrangement, building, cfg, SegmentRasteriser.get(), LOD13,
          building.rmse_lod13, building.volume_lod13, building.val3dity_lod13);
    }

    if (cfg->lod_22) {
      building.multisolids_lod22 = extrude_lod22(
          arrangement, building, cfg, SegmentRasteriser.get(), LOD22,
          building.rmse_lod22, building.volume_lod22, building.val3dity_lod22);
      compute_mesh_properties(
          building.multisolids_lod12, building.multisolids_lod13,
          building.multisolids_lod22, building.z_offset, cfg);
    }
    timings["extrude"] = std::chrono::high_resolution_clock::now() - t0;

    std::string timings_str =
        fmt::format("[reconstructor t] {} (", building.jsonl_path.string());
    for (const auto& [key, value] : timings) {
      auto ms = static_cast<int>(
          std::chrono::duration_cast<std::chrono::milliseconds>(value).count());
      timings_str += fmt::format("({}, {}),", key, ms);
    }
    logger.debug("{})", timings_str);
  }
}
