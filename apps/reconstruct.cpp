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
#include "partitioning/ArrangementOptimiser.hpp"
#include "partitioning/ArrangementDissolver.hpp"
#include "partitioning/ArrangementSnapper.hpp"
#include "partitioning/ArrangementExtruder.hpp"
#include "partitioning/MeshTriangulator.hpp"
#include "partitioning/PC2MeshDistCalculator.hpp"

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
template <>
struct rerun::CollectionAdapter<rerun::Position3D, roofer::TriangleCollection> {
  /// Borrow for non-temporary.
  Collection<rerun::Position3D> operator()(const roofer::TriangleCollection& container) {
      return Collection<rerun::Position3D>::borrow(container[0].data(), container.vertex_count());
  }

  // Do a full copy for temporaries (otherwise the data might be deleted when the temporary is destroyed).
  Collection<rerun::Position3D> operator()(roofer::TriangleCollection&& container) {
      std::vector<rerun::Position3D> positions(container.size());
      memcpy(positions.data(), container[0].data(), container.vertex_count() * sizeof(roofer::arr3f));
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

  std::string path_pointcloud = "output/wippolder/objects/503100000000296/crop/503100000000296_pointcloud.las";
  std::string path_footprint = "output/wippolder/objects/503100000000296/crop/503100000000296.gpkg";
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

  auto PlaneDetector = roofer::detection::createPlaneDetector();
  PlaneDetector->detect(points_roof);
  spdlog::info("Completed PlaneDetector (roof), found {} roofplanes", PlaneDetector->pts_per_roofplane.size());
  
  auto PlaneDetector_ground = roofer::detection::createPlaneDetector();
  PlaneDetector_ground->detect(points_ground);
  spdlog::info("Completed PlaneDetector (ground), found {} groundplanes", PlaneDetector_ground->pts_per_roofplane.size());
  
  rec.log("world/segmented_points", 
    rerun::Collection{rerun::components::AnnotationContext{
      rerun::datatypes::AnnotationInfo(0, "no plane", rerun::datatypes::Rgba32(30,30,30))
    }}
  );
  rec.log("world/segmented_points", rerun::Points3D(points_roof).with_class_ids(PlaneDetector->plane_id));

  auto AlphaShaper = roofer::detection::createAlphaShaper();
  AlphaShaper->compute(PlaneDetector->pts_per_roofplane);
  spdlog::info("Completed AlphaShaper (roof), found {} rings, {} labels", AlphaShaper->alpha_rings.size(), AlphaShaper->roofplane_ids.size());
  rec.log("world/alpha_rings_roof", rerun::LineStrips3D(AlphaShaper->alpha_rings).with_class_ids(AlphaShaper->roofplane_ids));
  
  auto AlphaShaper_ground = roofer::detection::createAlphaShaper();
  AlphaShaper_ground->compute(PlaneDetector_ground->pts_per_roofplane);
  spdlog::info("Completed AlphaShaper (ground), found {} rings, {} labels", AlphaShaper_ground->alpha_rings.size(), AlphaShaper_ground->roofplane_ids.size());
  rec.log("world/alpha_rings_ground", rerun::LineStrips3D(AlphaShaper_ground->alpha_rings).with_class_ids(AlphaShaper_ground->roofplane_ids));

  auto LineDetector = roofer::detection::createLineDetector();
  LineDetector->detect(AlphaShaper->alpha_rings, AlphaShaper->roofplane_ids, PlaneDetector->pts_per_roofplane);
  spdlog::info("Completed LineDetector");
  rec.log("world/boundary_lines", rerun::LineStrips3D(LineDetector->edge_segments));

  auto PlaneIntersector = roofer::detection::createPlaneIntersector();
  PlaneIntersector->compute(PlaneDetector->pts_per_roofplane, PlaneDetector->plane_adjacencies);
  spdlog::info("Completed PlaneIntersector");
  rec.log("world/intersection_lines", rerun::LineStrips3D(PlaneIntersector->segments));
  
  auto LineRegulariser = roofer::detection::createLineRegulariser();
  LineRegulariser->compute(LineDetector->edge_segments, PlaneIntersector->segments);
  spdlog::info("Completed LineRegulariser");
  rec.log("world/regularised_lines", rerun::LineStrips3D(LineRegulariser->regularised_edges));
  
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

  Arrangement_2 arrangement;
  auto ArrangementBuilder = roofer::detection::createArrangementBuilder();
  ArrangementBuilder->compute(
      arrangement,
      footprints[0],
      LineRegulariser->exact_regularised_edges
  );
  spdlog::info("Completed ArrangementBuilder");
  spdlog::info("Roof partition has {} faces", arrangement.number_of_faces());
  rec.log("world/initial_partition", rerun::LineStrips3D( roofer::detection::arr2polygons(arrangement) ));

  auto ArrangementOptimiser = roofer::detection::createArrangementOptimiser();
  ArrangementOptimiser->compute(
      arrangement,
      SegmentRasteriser->heightfield,
      PlaneDetector->pts_per_roofplane,
      PlaneDetector_ground->pts_per_roofplane
  );
  spdlog::info("Completed ArrangementOptimiser");
  // rec.log("world/optimised_partition", rerun::LineStrips3D( roofer::detection::arr2polygons(arrangement) ));

  auto ArrangementDissolver = roofer::detection::createArrangementDissolver();
  ArrangementDissolver->compute(arrangement,SegmentRasteriser->heightfield);
  spdlog::info("Completed ArrangementDissolver");
  spdlog::info("Roof partition has {} faces", arrangement.number_of_faces());
  rec.log("world/ArrangementDissolver", rerun::LineStrips3D( roofer::detection::arr2polygons(arrangement) ));
  auto ArrangementSnapper = roofer::detection::createArrangementSnapper();
  ArrangementSnapper->compute(arrangement);
  spdlog::info("Completed ArrangementSnapper");
  // rec.log("world/ArrangementSnapper", rerun::LineStrips3D( roofer::detection::arr2polygons(arrangement) ));
  
  auto ArrangementExtruder = roofer::detection::createArrangementExtruder();
  ArrangementExtruder->compute(arrangement, floor_elevation);
  spdlog::info("Completed ArrangementExtruder");
  rec.log("world/ArrangementExtruder", rerun::LineStrips3D(ArrangementExtruder->faces).with_class_ids(ArrangementExtruder->labels));

  auto MeshTriangulator = roofer::detection::createMeshTriangulatorLegacy();
  MeshTriangulator->compute(ArrangementExtruder->multisolid);
  spdlog::info("Completed MeshTriangulator");
  rec.log("world/MeshTriangulator", rerun::Mesh3D(MeshTriangulator->triangles).with_vertex_normals(MeshTriangulator->normals).with_class_ids(MeshTriangulator->ring_ids));
 
  auto PC2MeshDistCalculator = roofer::detection::createPC2MeshDistCalculator();
  PC2MeshDistCalculator->compute(PlaneDetector->pts_per_roofplane, MeshTriangulator->multitrianglecol, MeshTriangulator->ring_ids);
  spdlog::info("Completed PC2MeshDistCalculator. RMSE={}", PC2MeshDistCalculator->rms_error);
  // rec.log("world/PC2MeshDistCalculator", rerun::Mesh3D(PC2MeshDistCalculator->triangles).with_vertex_normals(MeshTriangulator->normals).with_class_ids(MeshTriangulator->ring_ids));

}