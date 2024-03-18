#include "projHelper.hpp"
#include "io/PointCloudReader.hpp"
#include "detection/ShapeDetector.hpp"

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

  PointReader->open(path_pointcloud);
  spdlog::info("Reading pointcloud from {}", path_pointcloud);
  roofer::vec1i classification;
  roofer::PointCollection points;
  roofer::AttributeVecMap attributes;
  PointReader->readPointCloud(points, &classification);

  spdlog::info("Read {} points", points.size());


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

  spdlog::info("Start plane detection");
  auto PlaneDetector = roofer::detection::createPlaneDetector();
  PlaneDetector->detect(points);
  spdlog::info("Completed plane detection [{}]", PlaneDetector->plane_id.size());
  
  roofer::vec1i plane_id = PlaneDetector->plane_id;
  
  rec.log("world/segmented_points", 
    rerun::Collection{rerun::components::AnnotationContext{
      rerun::datatypes::AnnotationInfo(0, "no plane", rerun::datatypes::Rgba32(0,0,0))
    }}
  );
  rec.log("world/segmented_points", rerun::Points3D(points).with_class_ids(plane_id));
  spdlog::info("Logged plane detection result");
  // Log the "my_points" entity with our data, using the `Points3D` archetype.
  // rec.log("my_points", rerun::Points3D(pts).with_colors(colors).with_radii({0.5f}));

}