enum LOD { LOD12 = 12, LOD13 = 13, LOD22 = 22 };

std::unordered_map<int, roofer::Mesh> extrude(
    roofer::Arrangement_2 arrangement, const float& floor_elevation,
#ifdef RF_USE_VAL3DITY
    std::vector<std::optional<std::string> >& attr_val3dity,
#endif
    float& rmse,
    roofer::reconstruction::SegmentRasteriserInterface* SegmentRasteriser,
    roofer::reconstruction::PlaneDetectorInterface* PlaneDetector, LOD lod) {
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
       .dissolve_all_interior = dissolve_all_interior});
  logger.info("Completed ArrangementDissolver");
  logger.info("Roof partition has {} faces", arrangement.number_of_faces());
#ifdef RF_USE_RERUN
  rec.log(
      worldname + "ArrangementDissolver",
      rerun::LineStrips3D(roofer::reconstruction::arr2polygons(arrangement)));
#endif
  auto ArrangementSnapper = roofer::reconstruction::createArrangementSnapper();
  ArrangementSnapper->compute(arrangement);
  logger.info("Completed ArrangementSnapper");
#ifdef RF_USE_RERUN
// rec.log(worldname+"ArrangementSnapper", rerun::LineStrips3D(
// roofer::reconstruction::arr2polygons(arrangement) ));
#endif

  auto ArrangementExtruder =
      roofer::reconstruction::createArrangementExtruder();
  ArrangementExtruder->compute(arrangement, floor_elevation,
                               {.LoD2 = extrude_LoD2});
  logger.info("Completed ArrangementExtruder");
#ifdef RF_USE_RERUN
  rec.log(worldname + "ArrangementExtruder",
          rerun::LineStrips3D(ArrangementExtruder->faces)
              .with_class_ids(ArrangementExtruder->labels));
#endif

  auto MeshTriangulator =
      roofer::reconstruction::createMeshTriangulatorLegacy();
  MeshTriangulator->compute(ArrangementExtruder->multisolid);
  logger.info("Completed MeshTriangulator");
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
  logger.info("Completed PC2MeshDistCalculator. RMSE={}",
              PC2MeshDistCalculator->rms_error);
#ifdef RF_USE_RERUN
// rec.log(worldname+"PC2MeshDistCalculator",
// rerun::Mesh3D(PC2MeshDistCalculator->triangles).with_vertex_normals(MeshTriangulator->normals).with_class_ids(MeshTriangulator->ring_ids));
#endif

#ifdef RF_USE_VAL3DITY
  auto Val3dator = roofer::reconstruction::createVal3dator();
  Val3dator->compute(ArrangementExtruder->multisolid);
  attr_val3dity.push_back(Val3dator->errors.front());
  logger.info("Completed Val3dator. Errors={}", Val3dator->errors.front());
#endif

  return ArrangementExtruder->multisolid;
}

void reconstruct_building(BuildingObject& building) {
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
  logger.info("{} ground points and {} roof points", points_ground.size(),
              points_roof.size());

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
  PlaneDetector->detect(points_roof);
  logger.info("Completed PlaneDetector (roof), found {} roofplanes",
              PlaneDetector->pts_per_roofplane.size());

  building.roof_type = PlaneDetector->roof_type;
  building.roof_elevation_50p = PlaneDetector->roof_elevation_50p;
  building.roof_elevation_70p = PlaneDetector->roof_elevation_70p;
  building.roof_elevation_min = PlaneDetector->roof_elevation_min;
  building.roof_elevation_max = PlaneDetector->roof_elevation_max;

  bool pointcloud_insufficient = PlaneDetector->roof_type == "no points" ||
                                 PlaneDetector->roof_type == "no planes";
  if (pointcloud_insufficient) {
    building.was_skipped = true;
    // TODO: return building status pointcloud_insufficient
    // CityJsonWriter->write(path_output_jsonl, footprints, nullptr, nullptr,
    //                       nullptr, attributes);
    return;
  }

  auto PlaneDetector_ground = roofer::reconstruction::createPlaneDetector();
  PlaneDetector_ground->detect(points_ground);
  logger.info("Completed PlaneDetector (ground), found {} groundplanes",
              PlaneDetector_ground->pts_per_roofplane.size());

#ifdef RF_USE_RERUN
  rec.log("world/segmented_points",
          rerun::Collection{rerun::components::AnnotationContext{
              rerun::datatypes::AnnotationInfo(
                  0, "no plane", rerun::datatypes::Rgba32(30, 30, 30))}});
  rec.log("world/segmented_points",
          rerun::Points3D(points_roof).with_class_ids(PlaneDetector->plane_id));
#endif

  // check skip_attribute
  building.was_skipped = building.skip;
  logger.info("Skip = {}", building.skip);

  if (building.skip) {
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
    // logger.info("Completed CityJsonWriter to {}", path_output_jsonl);
  } else {
    auto AlphaShaper = roofer::reconstruction::createAlphaShaper();
    AlphaShaper->compute(PlaneDetector->pts_per_roofplane);
    logger.info("Completed AlphaShaper (roof), found {} rings, {} labels",
                AlphaShaper->alpha_rings.size(),
                AlphaShaper->roofplane_ids.size());
#ifdef RF_USE_RERUN
    rec.log("world/alpha_rings_roof",
            rerun::LineStrips3D(AlphaShaper->alpha_rings)
                .with_class_ids(AlphaShaper->roofplane_ids));
#endif

    auto AlphaShaper_ground = roofer::reconstruction::createAlphaShaper();
    AlphaShaper_ground->compute(PlaneDetector_ground->pts_per_roofplane);
    logger.info("Completed AlphaShaper (ground), found {} rings, {} labels",
                AlphaShaper_ground->alpha_rings.size(),
                AlphaShaper_ground->roofplane_ids.size());
#ifdef RF_USE_RERUN
    rec.log("world/alpha_rings_ground",
            rerun::LineStrips3D(AlphaShaper_ground->alpha_rings)
                .with_class_ids(AlphaShaper_ground->roofplane_ids));
#endif

    auto LineDetector = roofer::reconstruction::createLineDetector();
    LineDetector->detect(AlphaShaper->alpha_rings, AlphaShaper->roofplane_ids,
                         PlaneDetector->pts_per_roofplane);
    logger.info("Completed LineDetector");
#ifdef RF_USE_RERUN
    rec.log("world/boundary_lines",
            rerun::LineStrips3D(LineDetector->edge_segments));
#endif

    auto PlaneIntersector = roofer::reconstruction::createPlaneIntersector();
    PlaneIntersector->compute(PlaneDetector->pts_per_roofplane,
                              PlaneDetector->plane_adjacencies);
    logger.info("Completed PlaneIntersector");
#ifdef RF_USE_RERUN
    rec.log("world/intersection_lines",
            rerun::LineStrips3D(PlaneIntersector->segments));
#endif

    auto LineRegulariser = roofer::reconstruction::createLineRegulariser();
    LineRegulariser->compute(LineDetector->edge_segments,
                             PlaneIntersector->segments);
    logger.info("Completed LineRegulariser");
#ifdef RF_USE_RERUN
    rec.log("world/regularised_lines",
            rerun::LineStrips3D(LineRegulariser->regularised_edges));
#endif

    auto SegmentRasteriser = roofer::reconstruction::createSegmentRasteriser();
    SegmentRasteriser->compute(AlphaShaper->alpha_triangles,
                               AlphaShaper_ground->alpha_triangles);
    logger.info("Completed SegmentRasteriser");

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
    logger.info("Completed ArrangementBuilder");
    logger.info("Roof partition has {} faces", arrangement.number_of_faces());
#ifdef RF_USE_RERUN
    rec.log(
        "world/initial_partition",
        rerun::LineStrips3D(roofer::reconstruction::arr2polygons(arrangement)));
#endif

    auto ArrangementOptimiser =
        roofer::reconstruction::createArrangementOptimiser();
    ArrangementOptimiser->compute(arrangement, SegmentRasteriser->heightfield,
                                  PlaneDetector->pts_per_roofplane,
                                  PlaneDetector_ground->pts_per_roofplane);
    logger.info("Completed ArrangementOptimiser");
// rec.log("world/optimised_partition", rerun::LineStrips3D(
// roofer::reconstruction::arr2polygons(arrangement) ));

// LoDs
// attributes to be filled during reconstruction
#ifdef RF_USE_VAL3DITY
    auto& attr_val3dity_lod12 =
        attributes.insert_vec<std::string>("b3_val3dity_lod12");
#endif
    building.multisolids_lod12 =
        extrude(arrangement, building.h_ground,
#ifdef RF_USE_VAL3DITY
                attr_val3dity_lod12,
#endif
                building.rmse_lod12, SegmentRasteriser.get(),
                PlaneDetector.get(), LOD12);
#ifdef RF_USE_VAL3DITY
    auto& attr_val3dity_lod13 =
        attributes.insert_vec<std::string>("b3_val3dity_lod13");
#endif
    building.multisolids_lod13 =
        extrude(arrangement, building.h_ground,
#ifdef RF_USE_VAL3DITY
                attr_val3dity_lod13,
#endif
                building.rmse_lod13, SegmentRasteriser.get(),
                PlaneDetector.get(), LOD13);
#ifdef RF_USE_VAL3DITY
    auto& attr_val3dity_lod22 =
        attributes.insert_vec<std::string>("b3_val3dity_lod22");
#endif
    building.multisolids_lod22 =
        extrude(arrangement, building.h_ground,
#ifdef RF_USE_VAL3DITY
                attr_val3dity_lod22,
#endif
                building.rmse_lod22, SegmentRasteriser.get(),
                PlaneDetector.get(), LOD22);
  }
}
