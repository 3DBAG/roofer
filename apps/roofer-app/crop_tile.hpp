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
#pragma once
bool crop_tile(const roofer::TBox<double>& tile,
               std::vector<InputPointcloud>& input_pointclouds,
               BuildingTile& output_building_tile, const RooferConfig& cfg,
               const roofer::io::SpatialReferenceSystemInterface* srs) {
  auto& logger = roofer::logger::Logger::get_logger();

  auto& pj = output_building_tile.proj_helper;
  auto vector_reader = roofer::io::createVectorReaderOGR(*pj);
  auto vector_writer = roofer::io::createVectorWriterOGR(*pj);
  auto PointCloudCropper = roofer::io::createPointCloudCropper(*pj);
  auto RasterWriter = roofer::io::createRasterWriterGDAL(*pj);
  auto vector_ops = roofer::misc::createVector2DOpsGEOS(*pj);
  auto LASWriter = roofer::io::createLASWriter(*pj);

  // logger.info("region_of_interest.has_value()? {}",
  // region_of_interest.has_value()); if(region_of_interest.has_value())
  vector_reader->layer_name = cfg.layer_name;
  vector_reader->layer_id = cfg.layer_id;
  vector_reader->attribute_filter = cfg.attribute_filter;
  vector_reader->open(cfg.source_footprints);
  vector_reader->region_of_interest = tile;
  std::vector<roofer::LinearRing> footprints;
  roofer::AttributeVecMap attributes;
  vector_reader->readPolygons(footprints, &attributes);

  const unsigned N_fp = footprints.size();
  if (N_fp == 0) {
    return false;
  }

  if (!pj->data_offset.has_value()) {
    logger.error("No data offset set after reading inputs");
    exit(1);
    return false;
  }

  // get yoc attribute vector (nullptr if it does not exist)
  auto yoc_vec = attributes.get_if<int>(cfg.yoc_attribute);
  if (!cfg.yoc_attribute.empty() && !yoc_vec) {
    logger.warning("yoc_attribute '{}' not found in input footprints",
                   cfg.yoc_attribute);
  }

  // simplify + buffer footprints
  logger.info("Simplifying and buffering footprints...");
  if (cfg.simplify) vector_ops->simplify_polygons(footprints);
  auto buffered_footprints = footprints;
  vector_ops->buffer_polygons(buffered_footprints);

  // compute true extent that includes all buffered footprints
  roofer::Box polygon_extent;
  for (auto& buf_ring : buffered_footprints) {
    polygon_extent.add(buf_ring.box());
  }

  // transform back to input coordinates
  roofer::TBox<double> polygon_extent_untransformed;
  polygon_extent_untransformed.add(pj->coord_transform_rev(
      polygon_extent.pmin[0], polygon_extent.pmin[1], polygon_extent.pmin[2]));
  polygon_extent_untransformed.add(pj->coord_transform_rev(
      polygon_extent.pmax[0], polygon_extent.pmax[1], polygon_extent.pmax[2]));

  // Crop all pointclouds
  for (auto& ipc : input_pointclouds) {
    logger.info("Cropping pointcloud {}...", ipc.name);

    auto intersecting_files = ipc.rtree->query(polygon_extent_untransformed);

    std::vector<std::string> lasfiles;
    for (auto* file_extent_ : intersecting_files) {
      auto* file_extent = static_cast<fileExtent*>(file_extent_);
      lasfiles.push_back(file_extent->first);
    }

    PointCloudCropper->process(
        lasfiles, footprints, buffered_footprints, ipc.building_clouds,
        ipc.ground_elevations, ipc.acquisition_years,
        ipc.pointcloud_insufficient, polygon_extent,
        {.ground_class = ipc.grnd_class,
         .building_class = ipc.bld_class,
         .use_acquisition_year = static_cast<bool>(yoc_vec)});
    if (ipc.date != 0) {
      logger.info("Overriding acquisition year from config file");
      std::fill(ipc.acquisition_years.begin(), ipc.acquisition_years.end(),
                ipc.date);
    }
  }

  // create force_lod11 vector, initialize with user input and area check
  // auto& force_lod11_vec = attributes.insert_vec<bool>(cfg.a_force_lod11);
  std::vector<std::optional<bool>> force_lod11_vec;
  force_lod11_vec.resize(N_fp, false);

  if (auto user_force_lod11_vec =
          attributes.get_if<bool>(cfg.force_lod11_attribute)) {
    force_lod11_vec = *user_force_lod11_vec;
  }
  for (size_t i = 0; i < N_fp; ++i) {
    force_lod11_vec[i] =
        *force_lod11_vec[i] ||
        std::fabs(footprints[i].signed_area()) > cfg.lod11_fallback_area;
  }

  // compute rasters
  // thin
  // compute nodata maxcircle
  for (auto& ipc : input_pointclouds) {
    logger.info("Analysing pointcloud {}...", ipc.name);
    ipc.nodata_radii.resize(N_fp);
    ipc.building_rasters.resize(N_fp);
    ipc.nodata_fractions.resize(N_fp);
    ipc.pt_densities.resize(N_fp);
    ipc.is_glass_roof.resize(N_fp);
    ipc.roof_elevations.resize(N_fp);
    ipc.lod11_forced.resize(N_fp);
    ipc.pointcloud_insufficient.resize(N_fp);
    if (cfg.write_index) ipc.nodata_circles.resize(N_fp);

    // auto& r_nodata = attributes.insert_vec<float>("r_nodata_"+ipc.name);
    roofer::arr2f nodata_c;
    for (unsigned i = 0; i < N_fp; ++i) {
      roofer::misc::RasterisePointcloud(ipc.building_clouds[i], footprints[i],
                                        ipc.building_rasters[i], cfg.cellsize,
                                        ipc.grnd_class, ipc.bld_class);
      ipc.nodata_fractions[i] =
          roofer::misc::computeNoDataFraction(ipc.building_rasters[i]);
      ipc.pt_densities[i] =
          roofer::misc::computePointDensity(ipc.building_rasters[i]);
      ipc.is_glass_roof[i] =
          roofer::misc::testForGlassRoof(ipc.building_rasters[i]);
      ipc.roof_elevations[i] =
          roofer::misc::computeRoofElevation(ipc.building_rasters[i], 0.7);

      auto target_density = cfg.ceil_point_density;
      bool do_force_lod11 =
          *force_lod11_vec[i] || ipc.force_lod11 || ipc.is_glass_roof[i];
      ipc.lod11_forced[i] = do_force_lod11;

      if (do_force_lod11) {
        target_density = cfg.lod11_fallback_density;
        // logger.info(
        //     "Applying extra thinning and skipping nodata circle calculation "
        //     "[force_lod11 = {}]",
        //     do_force_lod11);
      }

      roofer::misc::gridthinPointcloud(ipc.building_clouds[i],
                                       ipc.building_rasters[i]["cnt"],
                                       target_density);

      if (do_force_lod11) {
        ipc.nodata_radii[i] = 0;
      } else {
        try {
          roofer::misc::compute_nodata_circle(ipc.building_clouds[i],
                                              footprints[i],
                                              &ipc.nodata_radii[i], &nodata_c);
        } catch (const std::exception& e) {
          // logger.error(
          exit(1);
          //     "Failed to compute_nodata_circle in crop_tile for {}, setting "
          //     "ipc.nodata_radii[i] = 0, what(): {}",
          //     ipc.paths, e.what());
          ipc.nodata_radii[i] = 0;
        }
        if (cfg.write_index) {
          roofer::misc::draw_circle(ipc.nodata_circles[i], ipc.nodata_radii[i],
                                    nodata_c);
        }
      }
    }
  }

  // add raster stats attributes from PointCloudCropper to footprint attributes
  for (auto& ipc : input_pointclouds) {
    auto nodata_r =
        attributes.maybe_insert_vec<float>(cfg.a_nodata_r, ipc.name);
    auto nodata_frac =
        attributes.maybe_insert_vec<float>(cfg.a_nodata_frac, ipc.name);
    auto pt_density =
        attributes.maybe_insert_vec<float>(cfg.a_pt_density, ipc.name);
    if (nodata_r.has_value()) nodata_r->get().reserve(N_fp);
    if (nodata_frac.has_value()) nodata_frac->get().reserve(N_fp);
    if (pt_density.has_value()) pt_density->get().reserve(N_fp);
    for (unsigned i = 0; i < N_fp; ++i) {
      if (nodata_r.has_value()) nodata_r->get().push_back(ipc.nodata_radii[i]);
      if (nodata_frac.has_value())
        nodata_frac->get().push_back(ipc.nodata_fractions[i]);
      if (pt_density.has_value())
        pt_density->get().push_back(ipc.pt_densities[i]);
    }
  }

  // compute is_mutated attribute for first 2 pointclouds and add to footprint
  // attributes
  roofer::misc::selectPointCloudConfig select_pc_cfg;
  if (input_pointclouds.size() > 1) {
    // do roofer::misc::isMutated for each pair of consecutive pointclouds
    for (unsigned i = 0; i < input_pointclouds.size() - 1; ++i) {
      auto& ipc1 = input_pointclouds[i];
      auto& ipc2 = input_pointclouds[i + 1];
      auto is_mutated = attributes.maybe_insert_vec<bool>(
          cfg.a_is_mutated, ipc1.name + "_" + ipc2.name);
      if (is_mutated.has_value()) {
        is_mutated->get().reserve(N_fp);
        for (unsigned j = 0; j < N_fp; ++j) {
          is_mutated->get().push_back(roofer::misc::isMutated(
              ipc1.building_rasters[j], ipc2.building_rasters[j],
              select_pc_cfg.threshold_mutation_fraction,
              select_pc_cfg.threshold_mutation_difference));
        }
      }
    }
  }

  // select pointcloud and write out geoflow config + pointcloud / fp for each
  // building
  // logger.info("Selecting and writing pointclouds");
  auto bid_vec = attributes.get_if<std::string>(cfg.id_attribute);
  auto h_ground_fallback_vec =
      attributes.get_if<float>(cfg.h_terrain_attribute);
  auto h_roof_fallback_vec = attributes.get_if<float>(cfg.h_roof_attribute);
  auto pc_select = attributes.maybe_insert_vec<std::string>(cfg.a_pc_select);
  auto pc_source = attributes.maybe_insert_vec<std::string>(cfg.a_pc_source);
  auto pc_year = attributes.maybe_insert_vec<int>(cfg.a_pc_year);
  std::unordered_map<std::string, roofer::vec1s> jsonl_paths;
  std::string bid;
  bool only_write_selected = !cfg.output_all;
  for (unsigned i = 0; i < N_fp; ++i) {
    if (bid_vec) {
      bid = (*bid_vec)[i].value();
    } else {
      bid = std::to_string(i);
    }

    std::vector<roofer::misc::CandidatePointCloud> candidates;
    candidates.reserve(input_pointclouds.size());
    std::vector<roofer::misc::CandidatePointCloud> candidates_just_for_data;
    {
      int j = 0;
      for (auto& ipc : input_pointclouds) {
        auto cpc = roofer::misc::CandidatePointCloud{
            ipc.nodata_radii[i],
            ipc.nodata_fractions[i],
            ipc.building_rasters[i],
            yoc_vec ? (*yoc_vec)[i].value_or(-1) : -1,
            ipc.name,
            ipc.quality,
            ipc.acquisition_years[i],
            j++};
        if (ipc.select_only_for_date) {
          candidates_just_for_data.push_back(cpc);
        } else {
          candidates.push_back(cpc);
        }
        jsonl_paths.insert({ipc.name, roofer::vec1s{}});
      }
      jsonl_paths.insert({"", roofer::vec1s{}});
    }

    roofer::misc::PointCloudSelectResult sresult =
        roofer::misc::selectPointCloud(candidates, select_pc_cfg);
    const roofer::misc::CandidatePointCloud* selected =
        sresult.selected_pointcloud;

    // this is a sanity check and should never happen
    if (!selected) {
      logger.error("Unable to select pointcloud");
      exit(1);
      exit(1);
    }

    // check if yoc_attribute indicates this building to be built after selected
    // PC
    int yoc = yoc_vec ? (*yoc_vec)[i].value_or(-1) : -1;
    if (yoc != -1 && yoc > selected->date) {
      // force selection of latest pointcloud
      selected = getLatestPointCloud(candidates);
      sresult.explanation = roofer::misc::PointCloudSelectExplanation::_LATEST;
      // overrule if there was a more recent pointcloud with
      // select_only_for_date = true
      if (candidates_just_for_data.size()) {
        if (candidates_just_for_data[0].date > selected->date) {
          selected = &candidates_just_for_data[0];
          // sresult.explanation = roofer::PointCloudSelectExplanation::_LATEST;
        }
      }
    }

    if (pc_select.has_value()) {
      if (sresult.explanation ==
          roofer::misc::PointCloudSelectExplanation::PREFERRED_AND_LATEST)
        pc_select->get().push_back("PREFERRED_AND_LATEST");
      else if (sresult.explanation ==
               roofer::misc::PointCloudSelectExplanation::PREFERRED_NOT_LATEST)
        pc_select->get().push_back("PREFERRED_NOT_LATEST");
      else if (sresult.explanation ==
               roofer::misc::PointCloudSelectExplanation::LATEST_WITH_MUTATION)
        pc_select->get().push_back("LATEST_WITH_MUTATION");
      else if (sresult.explanation ==
               roofer::misc::PointCloudSelectExplanation::
                   _HIGHEST_YET_INSUFFICIENT_COVERAGE)
        pc_select->get().push_back("_HIGHEST_YET_INSUFFICIENT_COVERAGE");
      else if (sresult.explanation ==
               roofer::misc::PointCloudSelectExplanation::_LATEST) {
        pc_select->get().push_back("_LATEST");
        // // clear pc
        // ipc[selected->index].building_clouds[i].clear();
      } else
        pc_select->get().push_back("NONE");
    }
    if (pc_source.has_value()) {
      pc_source->get().push_back(selected->name);
    }
    if (pc_year.has_value()) {
      pc_year->get().push_back(selected->date);
    }

    // output to BuildingTile
    // set force_lod11 on building (for reconstruct) and attribute vec (for
    // output)
    {
      BuildingObject& building = output_building_tile.buildings.emplace_back();
      building.attribute_index = i;
      building.z_offset = (*pj->data_offset)[2];
      using TerrainStrategy = roofer::enums::TerrainStrategy;

      auto& points = input_pointclouds[selected->index].building_clouds[i];
      auto classification = points.attributes.get_if<int>("classification");
      for (size_t i = 0; i < points.size(); ++i) {
        if (input_pointclouds[selected->index].grnd_class ==
            (*classification)[i]) {
          building.pointcloud_ground.push_back(points[i]);
        } else if (input_pointclouds[selected->index].bld_class ==
                   (*classification)[i]) {
          building.pointcloud_building.push_back(points[i]);
        }
      }
      if (cfg.compute_pc_98p && points.size() > 0) {
        building.h_pc_98p =
            points.get_z_percentile(0.98) + (*pj->data_offset)[2];
      }
      building.footprint = footprints[i];
      auto h_ground_pc =
          input_pointclouds[selected->index].ground_elevations[i];
      if (cfg.h_terrain_strategy == TerrainStrategy::BUFFER_TILE) {
        if (h_ground_pc.has_value()) {
          building.h_ground = h_ground_pc.value();
        } else {
          building.h_ground = PointCloudCropper->get_min_terrain_elevation();
        }
      } else if (cfg.h_terrain_strategy == TerrainStrategy::BUFFER_USER) {
        if (h_ground_pc.has_value()) {
          building.h_ground = h_ground_pc.value();
        } else {
          if (h_ground_fallback_vec) {
            if ((*h_ground_fallback_vec)[i].has_value()) {
              building.h_ground = (*h_ground_fallback_vec)[i].value();
            } else {
              // fallback to min terrain elevation if no value is found
              building.h_ground =
                  PointCloudCropper->get_min_terrain_elevation();
              logger.warning(
                  "Falling back to minimum tile elevation for building {}",
                  bid);
            }
          } else {
            logger.error("No h_ground attribute found");
            exit(1);
          }
        }
      } else if (cfg.h_terrain_strategy == TerrainStrategy::USER) {
        if (h_ground_fallback_vec) {
          if ((*h_ground_fallback_vec)[i].has_value()) {
            building.h_ground = (*h_ground_fallback_vec)[i].value();
          } else {
            // fallback to min terrain elevation if no value is found
            building.h_ground = PointCloudCropper->get_min_terrain_elevation();
            logger.warning(
                "Falling back to minimum tile elevation for building {}", bid);
          }
        } else {
          logger.error("No h_ground attribute found");
          exit(1);
        }
      }
      if (h_roof_fallback_vec) {
        building.roof_h_fallback = (*h_roof_fallback_vec)[i];
      }

      building.h_pc_roof_70p =
          input_pointclouds[selected->index].roof_elevations[i];
      building.force_lod11 = input_pointclouds[selected->index].lod11_forced[i];
      building.pointcloud_insufficient =
          input_pointclouds[selected->index].pointcloud_insufficient[i];
      building.is_glass_roof =
          input_pointclouds[selected->index].is_glass_roof[i];

      if (input_pointclouds[selected->index].lod11_forced[i]) {
        building.extrusion_mode = ExtrusionMode::LOD11_FALLBACK;
        force_lod11_vec[i] = true;
      }

      building.jsonl_path = fmt::format(
          fmt::runtime(cfg.building_jsonl_file_spec), fmt::arg("bid", bid),
          fmt::arg("pc_name", input_pointclouds[selected->index].name),
          fmt::arg("path", cfg.output_path));
    }

    // update tile attributes
    auto a_force_lod11 = attributes.maybe_insert_vec<bool>(cfg.a_force_lod11);
    if (a_force_lod11.has_value()) {
      a_force_lod11->get().insert(a_force_lod11->get().begin(),
                                  force_lod11_vec.begin(),
                                  force_lod11_vec.end());
    }
    output_building_tile.attributes = attributes;

    if (cfg.write_crop_outputs) {
      {
        // fs::create_directories(fs::path(fname).parent_path());
        std::string fp_path = fmt::format(
            fmt::runtime(cfg.building_gpkg_file_spec), fmt::arg("bid", bid),
            fmt::arg("path", cfg.output_path));
        vector_writer->writePolygons(fp_path, srs, footprints, attributes, i,
                                     i + 1);

        size_t j = 0;
        for (auto& ipc : input_pointclouds) {
          if ((selected->index != j) && (only_write_selected)) {
            ++j;
            continue;
          };

          std::string pc_path = fmt::format(
              fmt::runtime(cfg.building_las_file_spec), fmt::arg("bid", bid),
              fmt::arg("pc_name", ipc.name), fmt::arg("path", cfg.output_path));
          std::string raster_path = fmt::format(
              fmt::runtime(cfg.building_raster_file_spec), fmt::arg("bid", bid),
              fmt::arg("pc_name", ipc.name), fmt::arg("path", cfg.output_path));
          std::string jsonl_path = fmt::format(
              fmt::runtime(cfg.building_jsonl_file_spec), fmt::arg("bid", bid),
              fmt::arg("pc_name", ipc.name), fmt::arg("path", cfg.output_path));

          if (cfg.write_rasters) {
            RasterWriter->writeBands(raster_path, ipc.building_rasters[i]);
          }

          LASWriter->write_pointcloud(input_pointclouds[j].building_clouds[i],
                                      srs, pc_path);

          if (!only_write_selected) {
            jsonl_paths[ipc.name].push_back(jsonl_path);
          }
          if (selected->index == j) {
            // set optimal jsonl path
            std::string jsonl_path =
                fmt::format(fmt::runtime(cfg.building_jsonl_file_spec),
                            fmt::arg("bid", bid), fmt::arg("pc_name", ""),
                            fmt::arg("path", cfg.output_path));
            // gf_config.insert_or_assign("OUTPUT_JSONL", jsonl_path);
            jsonl_paths[""].push_back(jsonl_path);
          }
          ++j;
        }
      };
      // write config
    }
  }

  // Write index output
  if (cfg.write_index) {
    std::string index_file = fmt::format(fmt::runtime(cfg.index_file_spec),
                                         fmt::arg("path", cfg.output_path));
    vector_writer->writePolygons(index_file, srs, footprints, attributes);

    // write nodata circles
    for (auto& ipc : input_pointclouds) {
      vector_writer->writePolygons(
          index_file + "_" + ipc.name + "_nodatacircle.gpkg", srs,
          ipc.nodata_circles, attributes);
    }
  }

  // write the txt containing paths to all jsonl features to be written by
  // reconstruct
  if (cfg.write_crop_outputs) {
    for (auto& [name, pathsvec] : jsonl_paths) {
      if (pathsvec.size() != 0) {
        std::string jsonl_list_file = fmt::format(
            fmt::runtime(cfg.jsonl_list_file_spec),
            fmt::arg("path", cfg.output_path), fmt::arg("pc_name", name));
        std::ofstream ofs;
        ofs.open(jsonl_list_file);
        for (auto& jsonl_p : pathsvec) {
          ofs << jsonl_p << "\n";
        }
        ofs.close();
      }
    }
  }
  // clear input_pointclouds data
  for (auto& ipc : input_pointclouds) {
    ipc.nodata_radii.clear();
    ipc.nodata_fractions.clear();
    ipc.pt_densities.clear();
    ipc.nodata_circles.clear();
    ipc.building_clouds.clear();
    ipc.building_rasters.clear();
    ipc.ground_elevations.clear();
    ipc.acquisition_years.clear();
  }

  return true;
}
