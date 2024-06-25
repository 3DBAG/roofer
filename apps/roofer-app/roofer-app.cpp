#include <deque>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
namespace fs = std::filesystem;

// common
#include <roofer/logger/logger.h>

#include <roofer/common/datastructures.hpp>

// crop
#include <roofer/io/PointCloudWriter.hpp>
#include <roofer/io/PointCloudReader.hpp>
#include <roofer/io/RasterWriter.hpp>
#include <roofer/io/StreamCropper.hpp>
#include <roofer/io/VectorReader.hpp>
#include <roofer/io/VectorWriter.hpp>
#include <roofer/misc/NodataCircleComputer.hpp>
#include <roofer/misc/PointcloudRasteriser.hpp>
#include <roofer/misc/Vector2DOps.hpp>
#include <roofer/misc/projHelper.hpp>
#include <roofer/misc/select_pointcloud.hpp>

// reconstruct
// #include <roofer/io/PointCloudReader.hpp>
// #include <roofer/io/VectorReader.hpp>
#include <roofer/misc/PC2MeshDistCalculator.hpp>
// #include <roofer/misc/projHelper.hpp>
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
#include <roofer/reconstruction/SimplePolygonExtruder.hpp>

// serialisation
#include <roofer/io/CityJsonWriter.hpp>

#include "argh.h"
#include "git.h"
#include "toml.hpp"
// roofer crop
// roofer reconstruct
// roofer tile

using fileExtent = std::pair<std::string, roofer::TBox<double>>;

struct InputPointcloud {
  std::string path;
  std::string name;
  int quality;
  int date = 0;
  int bld_class = 6;
  int grnd_class = 2;
  bool force_low_lod = false;
  bool select_only_for_date = false;

  roofer::vec1f nodata_radii;
  roofer::vec1f nodata_fractions;
  roofer::vec1f pt_densities;
  roofer::vec1b is_mutated;
  std::vector<roofer::LinearRing> nodata_circles;
  std::vector<roofer::PointCollection> building_clouds;
  std::vector<roofer::ImageMap> building_rasters;
  roofer::vec1f ground_elevations;
  roofer::vec1i acquisition_years;

  std::unique_ptr<roofer::misc::RTreeInterface> rtree;
  std::vector<fileExtent> file_extents;
};

struct BuildingObject {
  roofer::PointCollection pointcloud;
  roofer::LinearRing footprint;

  std::unordered_map<int, roofer::Mesh> multisolids_lod12;
  std::unordered_map<int, roofer::Mesh> multisolids_lod13;
  std::unordered_map<int, roofer::Mesh> multisolids_lod22;

  // set in crop
  std::string jsonl_path;
  float h_ground;
  bool skip;  // kas_warenhuis / low_lod

  // set in reconstruction
  std::string roof_type;
  float roof_elevation_50p;
  float roof_elevation_70p;
  float roof_elevation_min;
  float roof_elevation_max;
  float rmse_lod12;
  float rmse_lod13;
  float rmse_lod22;
  bool was_skipped;  // b3_reconstructie_onvolledig;
};

struct BuildingTile {
  std::deque<BuildingObject> buildings;
  roofer::AttributeVecMap attributes;
  // offset
  // extent
  roofer::TBox<double> extent;
};

struct RooferConfig {
  // footprint source parameters
  std::string source_footprints;
  std::string building_bid_attribute;
  std::string low_lod_attribute = "kas_warenhuis";
  std::string year_of_construction_attribute;

  // crop parameters
  float max_point_density = 20;
  float cellsize = 0.5;
  int low_lod_area = 69000;
  float max_point_density_low_lod = 5;

  bool write_crop_outputs = false;
  bool output_all = false;
  bool write_rasters = false;
  bool write_metadata = false;
  bool write_index = false;

  // general parameters
  std::optional<roofer::TBox<double>> region_of_interest;
  std::string output_crs;

  // crop output
  std::string building_toml_file_spec;
  std::string building_las_file_spec;
  std::string building_gpkg_file_spec;
  std::string building_raster_file_spec;
  std::string building_jsonl_file_spec;
  std::string jsonl_list_file_spec;
  std::string index_file_spec;
  std::string metadata_json_file_spec;
  std::string crop_output_path;

  // reconstruct
  //...
};

#include "crop_tile.hpp"
#include "reconstruct_building.hpp"

void print_help(std::string program_name) {
  // see http://docopt.org/
  std::cout << "Usage:" << "\n";
  std::cout << "   " << program_name;
  std::cout << " -c <file>" << "\n";
  std::cout << "Options:" << "\n";
  // std::cout << "   -v, --version                Print version information\n";
  std::cout << "   -h, --help                   Show this help message" << "\n";
  std::cout << "   -V, --version                Show version" << "\n";
  std::cout << "   -v, --verbose                Be more verbose" << "\n";
  std::cout << "   -c <file>, --config <file>   Config file" << "\n";
  std::cout << "   -r, --rasters                Output rasterised building "
               "pointclouds"
            << "\n";
  std::cout << "   -m, --metadata               Output metadata.json file"
            << "\n";
  std::cout << "   -i, --index                  Output index.gpkg file" << "\n";
  std::cout << "   -a, --all                    Output files for each "
               "candidate point cloud instead of only the optimal candidate"
            << "\n";
}

void print_version() {
  std::cout << fmt::format(
      "roofer {} ({}{}{})\n", git_Describe(),
      std::strcmp(git_Branch(), "main") ? ""
                                        : fmt::format("{}, ", git_Branch()),
      git_AnyUncommittedChanges() ? "dirty, " : "", git_CommitDate());
}

void read_config(const std::string& config_path, RooferConfig& cfg,
                 std::vector<InputPointcloud>& input_pointclouds) {
  auto& logger = roofer::logger::Logger::get_logger();
  toml::table config;
  config = toml::parse_file(config_path);

  auto tml_source_footprints =
      config["input"]["footprint"]["path"].value<std::string>();
  if (tml_source_footprints.has_value())
    cfg.source_footprints = *tml_source_footprints;

  auto id_attribute_ =
      config["input"]["footprint"]["id_attribute"].value<std::string>();
  if (id_attribute_.has_value()) cfg.building_bid_attribute = *id_attribute_;
  auto low_lod_attribute_ =
      config["input"]["low_lod_attribute"].value<std::string>();
  if (low_lod_attribute_.has_value())
    cfg.low_lod_attribute = *low_lod_attribute_;
  auto year_of_construction_attribute_ =
      config["input"]["year_of_construction_attribute"].value<std::string>();
  if (year_of_construction_attribute_.has_value())
    cfg.year_of_construction_attribute = *year_of_construction_attribute_;

  auto tml_pointclouds = config["input"]["pointclouds"];
  if (toml::array* arr = tml_pointclouds.as_array()) {
    // visitation with for_each() helps deal with heterogeneous data
    for (auto& el : *arr) {
      toml::table* tb = el.as_table();
      auto& pc = input_pointclouds.emplace_back();

      if (auto n = (*tb)["name"].value<std::string>(); n.has_value()) {
        pc.name = *n;
      }
      if (auto n = (*tb)["quality"].value<int>(); n.has_value()) {
        pc.quality = *n;
      }
      if (auto n = (*tb)["date"].value<int>(); n.has_value()) {
        pc.date = *n;
      }
      if (auto n = (*tb)["force_low_lod"].value<bool>(); n.has_value()) {
        pc.force_low_lod = *n;
      }
      if (auto n = (*tb)["select_only_for_date"].value<bool>(); n.has_value()) {
        pc.select_only_for_date = *n;
      }

      if (auto n = (*tb)["building_class"].value<int>(); n.has_value()) {
        pc.bld_class = *n;
      }
      if (auto n = (*tb)["ground_class"].value<int>(); n.has_value()) {
        pc.grnd_class = *n;
      }

      auto tml_path = (*tb)["path"].value<std::string>();
      if (tml_path.has_value()) {
        pc.path = *tml_path;
      }
    };
  }

  auto max_point_density_ =
      config["parameters"]["max_point_density"].value<float>();
  if (max_point_density_.has_value())
    cfg.max_point_density = *max_point_density_;

  auto cellsize_ = config["parameters"]["cellsize"].value<float>();
  if (cellsize_.has_value()) cfg.cellsize = *cellsize_;

  auto low_lod_area_ = config["parameters"]["low_lod_area"].value<int>();
  if (low_lod_area_.has_value()) cfg.low_lod_area = *low_lod_area_;

  if (toml::array* region_of_interest_ =
          config["parameters"]["region_of_interest"].as_array()) {
    if (region_of_interest_->size() == 4 &&
        (region_of_interest_->is_homogeneous(toml::node_type::floating_point) ||
         region_of_interest_->is_homogeneous(toml::node_type::integer))) {
      cfg.region_of_interest =
          roofer::TBox<double>{*region_of_interest_->get(0)->value<double>(),
                                *region_of_interest_->get(1)->value<double>(),
                                *region_of_interest_->get(2)->value<double>(),
                                *region_of_interest_->get(3)->value<double>()};
    } else {
      logger.error("Failed to read parameter.region_of_interest");
    }
  }
  auto building_toml_file_spec_ =
      config["output"]["building_toml_file"].value<std::string>();
  if (building_toml_file_spec_.has_value())
    cfg.building_toml_file_spec = *building_toml_file_spec_;

  auto building_las_file_spec_ =
      config["output"]["building_las_file"].value<std::string>();
  if (building_las_file_spec_.has_value())
    cfg.building_las_file_spec = *building_las_file_spec_;

  auto building_gpkg_file_spec_ =
      config["output"]["building_gpkg_file"].value<std::string>();
  if (building_gpkg_file_spec_.has_value())
    cfg.building_gpkg_file_spec = *building_gpkg_file_spec_;

  auto building_raster_file_spec_ =
      config["output"]["building_raster_file"].value<std::string>();
  if (building_raster_file_spec_.has_value())
    cfg.building_raster_file_spec = *building_raster_file_spec_;

  if (cfg.write_metadata) {
    auto metadata_json_file_spec_ =
        config["output"]["metadata_json_file"].value<std::string>();
    if (metadata_json_file_spec_.has_value())
      cfg.metadata_json_file_spec = *metadata_json_file_spec_;
  }

  auto building_jsonl_file_spec_ =
      config["output"]["building_jsonl_file"].value<std::string>();
  if (building_jsonl_file_spec_.has_value())
    cfg.building_jsonl_file_spec = *building_jsonl_file_spec_;

  auto index_file_spec_ = config["output"]["index_file"].value<std::string>();
  if (index_file_spec_.has_value()) cfg.index_file_spec = *index_file_spec_;

  auto jsonl_list_file_spec_ =
      config["output"]["jsonl_list_file"].value<std::string>();
  if (jsonl_list_file_spec_.has_value())
    cfg.jsonl_list_file_spec = *jsonl_list_file_spec_;

  auto output_path_ = config["output"]["path"].value<std::string>();
  if (output_path_.has_value()) cfg.crop_output_path = *output_path_;

  auto output_crs_ = config["output"]["crs"].value<std::string>();
  if (output_crs_.has_value()) cfg.output_crs = *output_crs_;
}

void get_las_extents(InputPointcloud& ipc) {;
  auto pj = roofer::misc::createProjHelper();
  for(auto& fp : roofer::find_filepaths(ipc.path, {".las", ".LAS", ".laz", ".LAZ"})) {
    auto PointReader = roofer::io::createPointCloudReaderLASlib(*pj);
    PointReader->open(fp);
    ipc.file_extents.push_back(std::make_pair(fp, PointReader->getExtent()));
  }
}

void create_tiles() {
  // auto& tile_geoms = vector_output("tile_geoms");

  // // get the intersection of footprints extent and pointcloud extent

  // // split up into tiles of predetermined cellsize

  // Box bbox;
  // for (size_t i=0; i<polygons.size(); ++i) {
  //   bbox.add(polygons.get<LinearRing&>(i).box());
  // }
  // auto grid = RasterTools::Raster(cellsize_, bbox.min()[0], bbox.max()[0], bbox.min()[1], bbox.max()[1]);

  // std::unordered_map<int, int> tile_cnts;
  // for (size_t i=0; i<polygons.size(); ++i) {
  //   auto c = polygons.get<LinearRing&>(i).box().center();
  //   int lc = (int) grid.getLinearCoord(c[0], c[1]);
  //   polygon_tile_ids.push_back(lc);
  //   tile_cnts[lc]++;
  // }

  // for(size_t col=0; col < grid.dimx_; ++col) {
  //   for(size_t row=0; row < grid.dimy_; ++row) {
  //     LinearRing g;
  //     g.push_back(arr3f{
  //       float(grid.minx_ + col*cellsize_),
  //       float(grid.miny_ + row*cellsize_),
  //       0            
  //     });
  //     g.push_back(arr3f{
  //       float(grid.minx_ + (col+1)*cellsize_),
  //       float(grid.miny_ + row*cellsize_),
  //       0            
  //     });
  //     g.push_back(arr3f{
  //       float(grid.minx_ + (col+1)*cellsize_),
  //       float(grid.miny_ + (row+1)*cellsize_),
  //       0            
  //     });
  //     g.push_back(arr3f{
  //       float(grid.minx_ + col*cellsize_),
  //       float(grid.miny_ + (row+1)*cellsize_),
  //       0            
  //     });
  //     auto lc = int(grid.getLinearCoord(row,col));
  //     tile_geom_cnts.push_back(tile_cnts[lc]);
  //     tile_geoms.push_back(g);
  //     tile_geom_ids.push_back(lc);
  //   }
  // }
}

int main(int argc, const char* argv[]) {
  auto cmdl = argh::parser({"-c", "--config"});

  cmdl.parse(argc, argv);
  std::string program_name = cmdl[0];
  auto& logger = roofer::logger::Logger::get_logger();

  // read cmdl options
  RooferConfig roofer_cfg;

  roofer_cfg.output_all = cmdl[{"-a", "--all"}];
  roofer_cfg.write_rasters = cmdl[{"-r", "--rasters"}];
  roofer_cfg.write_metadata = cmdl[{"-m", "--metadata"}];
  roofer_cfg.write_index = cmdl[{"-i", "--index"}];

  if (cmdl[{"-h", "--help"}]) {
    print_help(program_name);
    return EXIT_SUCCESS;
  }
  if (cmdl[{"-V", "--version"}]) {
    print_version();
    return EXIT_SUCCESS;
  }
  if (cmdl[{"-v", "--verbose"}]) {
    logger.set_level(roofer::logger::LogLevel::debug);
  } else {
    logger.set_level(roofer::logger::LogLevel::warning);
  }

  // Read configuration
  std::string config_path;
  std::vector<InputPointcloud> input_pointclouds;
  if (cmdl({"-c", "--config"}) >> config_path) {
    if (!fs::exists(config_path)) {
      logger.error("No such config file: {}", config_path);
      print_help(program_name);
      return EXIT_FAILURE;
    }
    logger.info("Reading configuration from file {}", config_path);
    try {
      read_config(config_path, roofer_cfg, input_pointclouds);
    } catch (const std::exception& e) {
      logger.error("Unable to parse config file {}.\n{}", config_path,
                   e.what());
      return EXIT_FAILURE;
    }
  } else {
    logger.error("No config file specified");
    return EXIT_FAILURE;
  }

  // Compute batch tile regions
  // we just create one tile for now
  std::deque<BuildingTile> building_tiles;

  for(auto& ipc: input_pointclouds) {
    get_las_extents(ipc);
    ipc.rtree = roofer::misc::createRTreeGEOS();
    for (auto& item : ipc.file_extents){
      ipc.rtree->insert(item.second, &item);
    }
  }

  auto pj = roofer::misc::createProjHelper();
  {
    auto VectorReader = roofer::io::createVectorReaderOGR(*pj);
    VectorReader->open(roofer_cfg.source_footprints);
    logger.info("region_of_interest.has_value()? {}",
                roofer_cfg.region_of_interest.has_value());
    logger.info("Reading footprints from {}", roofer_cfg.source_footprints);

    auto& building_tile = building_tiles.emplace_back();
    if (roofer_cfg.region_of_interest.has_value()) {
      // VectorReader->region_of_interest = *roofer_cfg.region_of_interest;
      building_tile.extent = *roofer_cfg.region_of_interest;
    } else {
      building_tile.extent = VectorReader->layer_extent;
    }
  }
  // Process tiles
  for (auto& building_tile : building_tiles) {
    // crop each tile
    crop_tile(building_tile.extent,  // tile extent
              input_pointclouds,     // input pointclouds
              building_tile,         // output building data
              roofer_cfg,            // configuration parameters
              pj.get());

    // reconstruct buildings
    for (auto& building : building_tile.buildings) {
      reconstruct_building(building);
    }

    // output reconstructed buildings
    auto CityJsonWriter = roofer::io::createCityJsonWriter(*pj);
    for (auto& building : building_tile.buildings) {
      std::vector<std::unordered_map<int, roofer::Mesh>> multisolidvec12,
          multisolidvec13, multisolidvec22;
      multisolidvec12.push_back(building.multisolids_lod12);
      multisolidvec13.push_back(building.multisolids_lod13);
      multisolidvec22.push_back(building.multisolids_lod22);
      std::vector<roofer::LinearRing> footprints{building.footprint};

      // TODO: fix attributes
      CityJsonWriter->write(building.jsonl_path, footprints, &multisolidvec12,
                            &multisolidvec13, &multisolidvec22,
                            building_tile.attributes);
      logger.info("Completed CityJsonWriter to {}", building.jsonl_path);
    }

    // buildings are finishes processing so they can be cleared
    building_tile.buildings.clear();
  }
}
