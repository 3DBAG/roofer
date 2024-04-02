#include <CGAL/IO/OBJ.h>

#include <rerun.hpp>

#include "cgal_utils.hpp"
#include "external/argh.h"
#include "external/toml.hpp"
#include "fmt/format.h"
#include "git.h"
#include "roofer.h"
#include "spdlog/spdlog.h"

typedef CGAL::Exact_predicates_inexact_constructions_kernel          K;
typedef CGAL::Point_3<K>                                              Point_3;

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
    std::strcmp(git_Branch(), "main") ? "" : fmt::format("{}, ", git_Branch()), 
    git_AnyUncommittedChanges() ? "dirty, " : "", 
    git_CommitDate()
  );
}

int main(int argc, const char * argv[]) {

  auto cmdl = argh::parser({ "-c", "--config" });

  cmdl.parse(argc, argv);
  std::string program_name = cmdl[0];

  std::string path_pointcloud = "./input.laz";
  std::string path_footprint = "./input.gpkg";
  float floor_elevation = -0.16899998486042023;

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

  // read inputs
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

  // reconstruct
  spdlog::info("Reconstructing");
  auto mesh = roofer::reconstruct_single_instance(points_roof, points_ground, footprints, floor_elevation);

  spdlog::info("Outputting to OBJ files");
  spdlog::info("Converting to CGAL mesh");
  CGAL::Surface_mesh<Point_3> cgalmesh = roofer::Mesh2CGALSurfaceMesh<Point_3>(mesh);

  spdlog::info("Writing to obj file");
  std::string filename("output.obj");
  CGAL::IO::write_OBJ(filename, cgalmesh);

}