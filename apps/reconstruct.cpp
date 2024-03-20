#include "projHelper.hpp"
#include "io/PointCloudReader.hpp"
#include "io/VectorReader.hpp"
#include "detection/PlaneDetector.hpp"
#include "detection/AlphaShaper.hpp"
#include "detection/LineDetector.hpp"
#include "detection/LineRegulariser.hpp"
#include "detection/PlaneIntersector.hpp"
#include "detection/SegmentRasteriser.hpp"
#include "partitioning/ArrangementBuilder.hpp"

#include "external/argh.h"
#include "external/toml.hpp"

#include "fmt/format.h"
#include "spdlog/spdlog.h"

#include "git.h"

#include <rerun.hpp>

// Adapters so we can log eigen vectors as rerun positions:
template <>
struct rerun::CollectionAdapter<rerun::Position3D, roofer::PointCollection> {
  /// Borrow for non-temporary.
  Collection<rerun::Position3D> operator()(const roofer::PointCollection& container) {
      return Collection<rerun::Position3D>::borrow(container.data(), container.size());
  }

  // Do a full copy for temporaries (otherwise the data might be deleted when the temporary is destroyed).
  Collection<rerun::Position3D> operator()(roofer::PointCollection&& container) {
      std::vector<rerun::Position3D> positions(container.size());
      memcpy(positions.data(), container.data(), container.size() * sizeof(roofer::arr3f));
      return Collection<rerun::Position3D>::take_ownership(std::move(positions));
  }
};

void print_help(std::string program_name) {
  // see http://docopt.org/
  fmt::print("Usage:\n");
  fmt::print("   {}", program_name);
  fmt::print("Options:\n");
  // std::cout << "   -v, --version                Print version information\n";
  fmt::print("   -h, --help                   Show this help message\n");
  fmt::print("   -V, --version                Show version\n");
  fmt::print("   -v, --verbose                Be more verbose\n");
}

void print_version() {
  fmt::print("roofer {} ({}{}{})\n", 
    git_Describe(), 
    git_Branch() == "main" ? "" : fmt::format("{}, ", git_Branch()), 
    git_AnyUncommittedChanges() ? "dirty, " : "", 
    git_CommitDate()
  );
}


int main(int argc, const char * argv[]) {

  auto cmdl = argh::parser({ "-c", "--config" });

  cmdl.parse(argc, argv);
  std::string program_name = cmdl[0];

  std::string path_pointcloud = "/Users/ravi/git/roofer/wippolder/output/wippolder/objects/503100000000296/crop/503100000000296_pointcloud.las";
  std::string path_footprint = "/Users/ravi/git/roofer/wippolder/output/wippolder/objects/503100000000296/crop/503100000000296.gpkg";

  // bool output_all = cmdl[{"-a", "--all"}];
  // bool write_rasters = cmdl[{"-r", "--rasters"}];
  // bool write_metadata = cmdl[{"-m", "--metadata"}];
  // bool write_index = cmdl[{"-i", "--index"}];
  bool verbose = cmdl[{"-v", "--verbose"}];
  bool version = cmdl[{"-V", "--version"}];

  if (cmdl[{"-h", "--help"}]) {
    print_help(program_name);
    return EXIT_SUCCESS;
  }
  if (version) {
    print_version();
    return EXIT_SUCCESS;
  }

  if (verbose) {
    spdlog::set_level(spdlog::level::debug);
  } else {
    spdlog::set_level(spdlog::level::warn);
  }

  auto pj = roofer::createProjHelper();
  auto PointReader = roofer::createPointCloudReaderLASlib(*pj);
  auto VectorReader = roofer::createVectorReaderOGR(*pj);

  VectorReader->open(path_footprint);
  std::vector<roofer::LinearRing> footprints;
  VectorReader->readPolygons(footprints);

  PointReader->open(path_pointcloud);
  spdlog::info("Reading pointcloud from {}", path_pointcloud);
  roofer::vec1i classification;
  roofer::PointCollection points, points_ground, points_roof;
  roofer::AttributeVecMap attributes;
  PointReader->readPointCloud(points, &classification);
  spdlog::info("Read {} points", points.size());

  // split into ground and roof points
  for (size_t i=0; i<points.size(); ++i) {
    if( 2 == classification[i] ) {
      points_ground.push_back(points[i]);
    } else if( 6 == classification[i] ) {
      points_roof.push_back(points[i]);
    }
  }

  spdlog::info("{} ground points and {} roof points", points_ground.size(), points_roof.size());
  
  // Create a new `RecordingStream` which sends data over TCP to the viewer process.
  const auto rec = rerun::RecordingStream("Roofer rerun test");
  // Try to spawn a new viewer instance.
  rec.spawn().exit_on_failure();

  rec.log("world/raw_points", 
    rerun::Collection{rerun::components::AnnotationContext{
      rerun::datatypes::AnnotationInfo(6, "BUILDING", rerun::datatypes::Rgba32(255,0,0)),
      rerun::datatypes::AnnotationInfo(2, "GROUND"),
      rerun::datatypes::AnnotationInfo(1, "UNCLASSIFIED"),
    }}
  );

  rec.log("world/raw_points", rerun::Points3D(points).with_class_ids(classification));

  spdlog::info("Start plane detection (roof)");
  auto PlaneDetector = roofer::detection::createPlaneDetector();
  PlaneDetector->detect(points_roof);
  spdlog::info("Completed plane detection (roof), found {} roofplanes", PlaneDetector->pts_per_roofplane.size());
  
  spdlog::info("Start plane detection (ground)");
  auto PlaneDetector_ground = roofer::detection::createPlaneDetector();
  PlaneDetector_ground->detect(points_ground);
  spdlog::info("Completed plane detection (ground), found {} groundplanes", PlaneDetector_ground->pts_per_roofplane.size());

  
  rec.log("world/segmented_points", 
    rerun::Collection{rerun::components::AnnotationContext{
      rerun::datatypes::AnnotationInfo(0, "no plane", rerun::datatypes::Rgba32(30,30,30))
    }}
  );
  // spdlog::info("pts {}, ids {}", points.size(), PlaneDetector->plane_id.size());
  rec.log("world/segmented_points", rerun::Points3D(points_roof).with_class_ids(PlaneDetector->plane_id));

  spdlog::info("Start AlphaShaper (roof)");
  auto AlphaShaper = roofer::detection::createAlphaShaper();
  AlphaShaper->compute(PlaneDetector->pts_per_roofplane);
  spdlog::info("Completed AlphaShaper (roof), found {} rings, {} labels", AlphaShaper->alpha_rings.size(), AlphaShaper->roofplane_ids.size());
  rec.log("world/alpha_rings_roof", rerun::LineStrips3D(AlphaShaper->alpha_rings).with_class_ids(AlphaShaper->roofplane_ids));
  
  spdlog::info("Start AlphaShaper (ground)");
  auto AlphaShaper_ground = roofer::detection::createAlphaShaper();
  AlphaShaper_ground->compute(PlaneDetector_ground->pts_per_roofplane);
  spdlog::info("Completed AlphaShaper (ground), found {} rings, {} labels", AlphaShaper_ground->alpha_rings.size(), AlphaShaper_ground->roofplane_ids.size());
  rec.log("world/alpha_rings_ground", rerun::LineStrips3D(AlphaShaper_ground->alpha_rings).with_class_ids(AlphaShaper_ground->roofplane_ids));

  spdlog::info("Start LineDetector");
  auto LineDetector = roofer::detection::createLineDetector();
  LineDetector->detect(AlphaShaper->alpha_rings, AlphaShaper->roofplane_ids, PlaneDetector->pts_per_roofplane);
  spdlog::info("Completed LineDetector");
  rec.log("world/boundary_lines", rerun::LineStrips3D(LineDetector->edge_segments));

  spdlog::info("Start PlaneIntersector");
  auto PlaneIntersector = roofer::detection::createPlaneIntersector();
  PlaneIntersector->compute(PlaneDetector->pts_per_roofplane, PlaneDetector->plane_adjacencies);
  spdlog::info("Completed PlaneIntersector");
  rec.log("world/intersection_lines", rerun::LineStrips3D(PlaneIntersector->segments));
  
  spdlog::info("Start LineRegulariser");
  auto LineRegulariser = roofer::detection::createLineRegulariser();
  LineRegulariser->compute(LineDetector->edge_segments, PlaneIntersector->segments);
  spdlog::info("Completed LineRegulariser");
  rec.log("world/regularised_lines", rerun::LineStrips3D(LineRegulariser->regularised_edges));
  
  spdlog::info("Start SegmentRasteriser");
  auto SegmentRasteriser = roofer::detection::createSegmentRasteriser();
  SegmentRasteriser->compute(
      AlphaShaper->alpha_triangles,
      AlphaShaper_ground->alpha_triangles
  );
  spdlog::info("Completed SegmentRasteriser");
  
  auto heightfield_copy = SegmentRasteriser->heightfield;
  heightfield_copy.set_nodata(0);
  rec.log("world/heightfield", rerun::DepthImage(
    {
      heightfield_copy.dimy_, 
      heightfield_copy.dimx_
    }, 
    *heightfield_copy.vals_)
  );

  spdlog::info("Start ArrangementBuilder");
  auto ArrangementBuilder = roofer::detection::createArrangementBuilder();
  ArrangementBuilder->compute(
      footprints[0],
      LineRegulariser->exact_regularised_edges
  );
  spdlog::info("Completed ArrangementBuilder");


}