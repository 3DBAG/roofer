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
// Ravi Peters

enum LOD { LOD12 = 12, LOD13 = 13, LOD22 = 22 };

std::unordered_map<int, roofer::Mesh> extrude(
    roofer::Arrangement_2 arrangement, const float& floor_elevation,
    const float& z_offset,
#ifdef RF_USE_VAL3DITY
    std::string& attr_val3dity,
#endif
    float& rmse, float& volume,
    roofer::reconstruction::SegmentRasteriserInterface* SegmentRasteriser,
    roofer::reconstruction::PlaneDetectorInterface* PlaneDetector, LOD lod,
    RooferConfig* rfcfg) {
  auto* cfg = &(rfcfg->rec);
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
#endif
  std::string worldname = fmt::format("world/lod{}/", (int)lod);

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
  rec.log(
      worldname + "ArrangementDissolver",
      rerun::LineStrips3D(roofer::reconstruction::arr2polygons(arrangement)));
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
  ArrangementExtruder->compute(arrangement, floor_elevation,
                               {.LoD2 = extrude_LoD2});
  // logger.debug("Completed ArrangementExtruder");
#ifdef RF_USE_RERUN
  rec.log(worldname + "ArrangementExtruder",
          rerun::LineStrips3D(ArrangementExtruder->faces)
              .with_class_ids(ArrangementExtruder->labels));
#endif

  auto MeshPropertyCalculator = roofer::misc::createMeshPropertyCalculator();
  for (auto& [label, mesh] : ArrangementExtruder->multisolid) {
    mesh.get_attributes().resize(mesh.get_polygons().size());
    MeshPropertyCalculator->compute_roof_height(
        mesh, {.z_offset = z_offset,
               .cellsize = rfcfg->cellsize,
               .h_50p = rfcfg->n["h_roof_50p"],
               .h_70p = rfcfg->n["h_roof_70p"],
               .h_min = rfcfg->n["h_roof_min"],
               .h_max = rfcfg->n["h_roof_max"]});
    if (lod == LOD22) {
      MeshPropertyCalculator->compute_roof_orientation(
          mesh, {.slope = rfcfg->n["slope"], .azimuth = rfcfg->n["azimuth"]});
    }
  }

  auto MeshTriangulator =
      roofer::reconstruction::createMeshTriangulatorLegacy();
  MeshTriangulator->compute(ArrangementExtruder->multisolid);
  volume = MeshTriangulator->volumes.at(0);
  // logger.debug("Completed MeshTriangulator");
#ifdef RF_USE_RERUN
  rec.log(worldname + "MeshTriangulator",
          rerun::Mesh3D(MeshTriangulator->triangles)
              .with_vertex_normals(MeshTriangulator->normals)
              .with_class_ids(MeshTriangulator->ring_ids));
#endif

  auto PC2MeshDistCalculator = roofer::misc::createPC2MeshDistCalculator();
  PC2MeshDistCalculator->compute(PlaneDetector->pts_per_roofplane,
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
  if (ArrangementExtruder->multisolid.size() > 0) {
    auto Val3dator = roofer::misc::createVal3dator();
    Val3dator->compute(ArrangementExtruder->multisolid);
    attr_val3dity = Val3dator->errors.front();
  }
  // logger.debug("Completed Val3dator. Errors={}", Val3dator->errors.front());
#endif

  return ArrangementExtruder->multisolid;
}

void reconstruct_building(BuildingObject& building, RooferConfig* rfcfg) {
  auto* cfg = &(rfcfg->rec);
  auto& logger = roofer::logger::Logger::get_logger();
  // split into ground and roof points
  roofer::PointCollection points, points_ground, points_roof;
  auto classification =
      building.pointcloud.attributes.get_if<int>("classification");
  for (size_t i = 0; i < building.pointcloud.size(); ++i) {
    if (2 == (*classification)[i]) {
      points_ground.push_back(building.pointcloud[i]);
    } else if (6 == (*classification)[i]) {
      points_roof.push_back(building.pointcloud[i]);
    }
  }
  // logger.debug("{} ground points and {} roof points", points_ground.size(),
  //  points_roof.size());

#ifdef RF_USE_RERUN
  // Create a new `RecordingStream` which sends data over TCP to the viewer
  // process.
  const auto rec = rerun::RecordingStream("Roofer rerun test");
  // Try to spawn a new viewer instance.
  rec.spawn().exit_on_failure();
  rec.set_global();
#endif

#ifdef RF_USE_RERUN
  rec.log("world/raw_points",
          rerun::Collection{rerun::components::AnnotationContext{
              rerun::datatypes::AnnotationInfo(
                  6, "BUILDING", rerun::datatypes::Rgba32(255, 0, 0)),
              rerun::datatypes::AnnotationInfo(2, "GROUND"),
              rerun::datatypes::AnnotationInfo(1, "UNCLASSIFIED"),
          }});
  rec.log("world/raw_points",
          rerun::Points3D(points).with_class_ids(classification));
#endif

  auto PlaneDetector = roofer::reconstruction::createPlaneDetector();
  PlaneDetector->detect(
      points_roof,
      {
          .metrics_plane_k = cfg->plane_detect_k,
          .metrics_plane_min_points = cfg->plane_detect_min_points,
          .metrics_plane_epsilon = cfg->plane_detect_epsilon,
          .metrics_plane_normal_threshold = cfg->plane_detect_normal_angle,
      });
  // logger.debug("Completed PlaneDetector (roof), found {} roofplanes",
  //  PlaneDetector->pts_per_roofplane.size());

  building.roof_type = PlaneDetector->roof_type;
  building.roof_elevation_50p = PlaneDetector->roof_elevation_50p;
  building.roof_elevation_70p = PlaneDetector->roof_elevation_70p;
  building.roof_elevation_min = PlaneDetector->roof_elevation_min;
  building.roof_elevation_max = PlaneDetector->roof_elevation_max;
  building.roof_n_planes = PlaneDetector->pts_per_roofplane.size();

  bool pointcloud_insufficient = PlaneDetector->roof_type == "no points" ||
                                 PlaneDetector->roof_type == "no planes";
  if (pointcloud_insufficient) {
    // building.was_skipped = true;
    // TODO: return building status pointcloud_insufficient
    // CityJsonWriter->write(path_output_jsonl, footprints, nullptr, nullptr,
    //                       nullptr, attributes);
    return;
  }

  auto PlaneDetector_ground = roofer::reconstruction::createPlaneDetector();
  PlaneDetector_ground->detect(points_ground);
  // logger.debug("Completed PlaneDetector (ground), found {} groundplanes",
  //  PlaneDetector_ground->pts_per_roofplane.size());

#ifdef RF_USE_RERUN
  rec.log("world/segmented_points",
          rerun::Collection{rerun::components::AnnotationContext{
              rerun::datatypes::AnnotationInfo(
                  0, "no plane", rerun::datatypes::Rgba32(30, 30, 30))}});
  rec.log("world/segmented_points",
          rerun::Points3D(points_roof).with_class_ids(PlaneDetector->plane_id));
#endif

  // check skip_attribute
  // logger.debug("force LoD1.1 = {}", building.force_lod11);

  if (building.force_lod11) {
    auto SimplePolygonExtruder =
        roofer::reconstruction::createSimplePolygonExtruder();
    SimplePolygonExtruder->compute(building.footprint, building.h_ground,
                                   PlaneDetector->roof_elevation_70p);
    std::vector<std::unordered_map<int, roofer::Mesh> > multisolidvec;
    building.multisolids_lod12 = SimplePolygonExtruder->multisolid;
    building.multisolids_lod13 = SimplePolygonExtruder->multisolid;
    building.multisolids_lod22 = SimplePolygonExtruder->multisolid;
    // TODO: return building status
    return;
    // multisolidvec.push_back(SimplePolygonExtruder->multisolid);
    // CityJsonWriter->write(path_output_jsonl, footprints, &multisolidvec,
    //                       &multisolidvec, &multisolidvec, attributes);
    // logger.debug("Completed CityJsonWriter to {}", path_output_jsonl);
  } else {
    auto AlphaShaper = roofer::reconstruction::createAlphaShaper();
    AlphaShaper->compute(PlaneDetector->pts_per_roofplane,
                         {.thres_alpha = cfg->thres_alpha});
    // logger.debug("Completed AlphaShaper (roof), found {} rings, {} labels",
    //  AlphaShaper->alpha_rings.size(),
    //  AlphaShaper->roofplane_ids.size());
#ifdef RF_USE_RERUN
    rec.log("world/alpha_rings_roof",
            rerun::LineStrips3D(AlphaShaper->alpha_rings)
                .with_class_ids(AlphaShaper->roofplane_ids));
#endif

    auto AlphaShaper_ground = roofer::reconstruction::createAlphaShaper();
    AlphaShaper_ground->compute(PlaneDetector_ground->pts_per_roofplane);
    // logger.debug("Completed AlphaShaper (ground), found {} rings, {} labels",
    //  AlphaShaper_ground->alpha_rings.size(),
    //  AlphaShaper_ground->roofplane_ids.size());
#ifdef RF_USE_RERUN
    rec.log("world/alpha_rings_ground",
            rerun::LineStrips3D(AlphaShaper_ground->alpha_rings)
                .with_class_ids(AlphaShaper_ground->roofplane_ids));
#endif

    auto LineDetector = roofer::reconstruction::createLineDetector();
    LineDetector->detect(AlphaShaper->alpha_rings, AlphaShaper->roofplane_ids,
                         PlaneDetector->pts_per_roofplane,
                         {.dist_thres = cfg->line_detect_epsilon});
    // logger.debug("Completed LineDetector");
#ifdef RF_USE_RERUN
    rec.log("world/boundary_lines",
            rerun::LineStrips3D(LineDetector->edge_segments));
#endif

    auto PlaneIntersector = roofer::reconstruction::createPlaneIntersector();
    PlaneIntersector->compute(PlaneDetector->pts_per_roofplane,
                              PlaneDetector->plane_adjacencies);
    // logger.debug("Completed PlaneIntersector");
#ifdef RF_USE_RERUN
    rec.log("world/intersection_lines",
            rerun::LineStrips3D(PlaneIntersector->segments));
#endif

    auto LineRegulariser = roofer::reconstruction::createLineRegulariser();
    LineRegulariser->compute(LineDetector->edge_segments,
                             PlaneIntersector->segments,
                             {.dist_threshold = cfg->thres_reg_line_dist,
                              .extension = cfg->thres_reg_line_ext});
    // logger.debug("Completed LineRegulariser");
#ifdef RF_USE_RERUN
    rec.log("world/regularised_lines",
            rerun::LineStrips3D(LineRegulariser->regularised_edges));
#endif

    auto SegmentRasteriser = roofer::reconstruction::createSegmentRasteriser();
    SegmentRasteriser->compute(
        AlphaShaper->alpha_triangles, AlphaShaper_ground->alpha_triangles,
        {.use_ground = !points_ground.empty() && cfg->clip_ground});
    // logger.debug("Completed SegmentRasteriser");

#ifdef RF_USE_RERUN
    auto heightfield_copy = SegmentRasteriser->heightfield;
    heightfield_copy.set_nodata(0);
    rec.log("world/heightfield",
            rerun::DepthImage({heightfield_copy.dimy_, heightfield_copy.dimx_},
                              *heightfield_copy.vals_));
#endif

    roofer::Arrangement_2 arrangement;
    auto ArrangementBuilder =
        roofer::reconstruction::createArrangementBuilder();
    ArrangementBuilder->compute(arrangement, building.footprint,
                                LineRegulariser->exact_regularised_edges);
    // logger.debug("Completed ArrangementBuilder");
    // logger.debug("Roof partition has {} faces",
    // arrangement.number_of_faces());
#ifdef RF_USE_RERUN
    rec.log(
        "world/initial_partition",
        rerun::LineStrips3D(roofer::reconstruction::arr2polygons(arrangement)));
#endif

    auto ArrangementOptimiser =
        roofer::reconstruction::createArrangementOptimiser();
    ArrangementOptimiser->compute(
        arrangement, SegmentRasteriser->heightfield,
        PlaneDetector->pts_per_roofplane,
        PlaneDetector_ground->pts_per_roofplane,
        {
            .data_multiplier = cfg->complexity_factor,
            .smoothness_multiplier = float(1. - cfg->complexity_factor),
            .use_ground = !points_ground.empty() && cfg->clip_ground,
        });
    // logger.debug("Completed ArrangementOptimiser");
    // rec.log("world/optimised_partition", rerun::LineStrips3D(
    // roofer::reconstruction::arr2polygons(arrangement) ));

    // LoDs
    // attributes to be filled during reconstruction
    // logger.debug("LoD={}", cfg->lod);
    if (cfg->lod == 0 || cfg->lod == 12) {
      building.multisolids_lod12 =
          extrude(arrangement, building.h_ground, building.z_offset,
#ifdef RF_USE_VAL3DITY
                  building.val3dity_lod12,
#endif
                  building.rmse_lod12, building.volume_lod12,
                  SegmentRasteriser.get(), PlaneDetector.get(), LOD12, rfcfg);
    }

    if (cfg->lod == 0 || cfg->lod == 13) {
      building.multisolids_lod13 =
          extrude(arrangement, building.h_ground, building.z_offset,
#ifdef RF_USE_VAL3DITY
                  building.val3dity_lod13,
#endif
                  building.rmse_lod13, building.volume_lod13,
                  SegmentRasteriser.get(), PlaneDetector.get(), LOD13, rfcfg);
    }

    if (cfg->lod == 0 || cfg->lod == 22) {
      building.multisolids_lod22 =
          extrude(arrangement, building.h_ground, building.z_offset,
#ifdef RF_USE_VAL3DITY
                  building.val3dity_lod22,
#endif
                  building.rmse_lod22, building.volume_lod22,
                  SegmentRasteriser.get(), PlaneDetector.get(), LOD22, rfcfg);
    }
  }
}
