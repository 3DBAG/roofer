#include "polyscope/polyscope.h"
#include "polyscope/point_cloud.h"
// #include "polyscope/surface_mesh.h"

#include "projHelper.hpp"
#include "io/VectorReader.hpp"
#include "io/StreamCropper.hpp"
#include "geometry/ShapeDetector.hpp"

std::vector<roofer::PointCollection> point_clouds;
std::unique_ptr<roofer::ShapeDetectorInterface> ShapeDetector;
int bid = 1;
float ransac_probability = 0.01;
int ransac_min_points = 15;
float ransac_epsilon = 0.2;
float ransac_cluster_epsilon = 0.5;
float ransac_normal_threshold = 0.8;
bool bid_changed = true;
// int old_bid_changed;

  void myCallback() {

    // Since options::openImGuiWindowForUserCallback == true by default, 
    // we can immediately start using ImGui commands to build a UI

    ImGui::PushItemWidth(100); // Make ui elements 100 pixels wide,
                              // instead of full width. Must have 
                              // matching PopItemWidth() below.

    int bid_old = bid;
    if(ImGui::DragInt("Point cloud #", &bid, 1.0f, 1, point_clouds.size()+1)) {
      polyscope::removeStructure("pc" + std::to_string(bid_old), false);
      bid_changed = true;
    }
    ImGui::InputFloat("probability", &ransac_probability);
    ImGui::InputInt("min_points", &ransac_min_points);
    ImGui::InputFloat("epsilon", &ransac_epsilon);
    ImGui::InputFloat("cluster_epsilon", &ransac_cluster_epsilon);
    ImGui::InputFloat("normal_threshold", &ransac_normal_threshold);

    if (ImGui::Button("Do RANSAC")) {
      // executes when button is pressed
      auto&pc = point_clouds[bid-1];
      roofer::vec1i labels;
      roofer::vec3f normals;
      auto nShapes = ShapeDetector->detectPlanes(
        pc, 
        normals, 
        labels,
        ransac_probability,
        ransac_min_points,
        ransac_epsilon,
        ransac_cluster_epsilon,
        ransac_normal_threshold
      );

      std::vector<std::array<double, 3>> randColor(nShapes);
      randColor[0] = {{0.0f,0.0f,0.0f}};
      for (size_t i = 1; i < nShapes; i++) {
        randColor[i] = {{polyscope::randomUnit(), polyscope::randomUnit(), polyscope::randomUnit()}};
      }

      std::vector<std::array<double, 3>> labelColors;
      labelColors.reserve(labels.size());
      for (auto& l : labels) {
        labelColors.push_back( randColor[l] );
      }

      std::string name = "pc" + std::to_string(bid);
      auto cloud = polyscope::registerPointCloud(name, pc);
      cloud->addScalarQuantity(name + " labels", labels);
      cloud->addVectorQuantity(name + " normals", normals);
      cloud->addColorQuantity(name + " label color", labelColors);
      
      if (bid_changed) {
        polyscope::view::resetCameraToHomeView();
        bid_changed = false;
      }
      
    }
    ImGui::SameLine();
    if (ImGui::Button("hi")) {
      polyscope::warning("hi");
    }

    ImGui::PopItemWidth();
  };

int main(int argc, const char * argv[]) {

  auto pj = roofer::createProjHelper();

  auto VectorReader = roofer::createVectorReaderOGR(*pj);
  auto PointCloudCropper = roofer::createPointCloudCropper(*pj);
  ShapeDetector = roofer::createShapeDetector();

  // std::string vector_file = "/home/ravi/git/gfc-building-reconstruction/single/test-data/wippolder.gpkg";
  // std::string las_source = "/home/ravi/git/gfc-building-reconstruction/single/test-data/wippolder.las";
  // std::string vector_file = "/mnt/Data/LocalData/Kadaster/true_ortho_experimenten/2021_LAZ_Leiden_Almere/DenHaag/bag_83000_455000.gpkg";
  // std::string las_source = "/mnt/Data/LocalData/Kadaster/true_ortho_experimenten/2021_LAZ_Leiden_Almere/DenHaag/83000_455000.laz";
  std::string vector_file = "/mnt/Data/LocalData/Kadaster/true_ortho_experimenten/2021_Eindhoven/LAZ/DSM_1500_4142.gpkg";
  std::string las_source = "/mnt/Data/LocalData/Kadaster/true_ortho_experimenten/2021_Eindhoven/LAZ/DSM_1500_4142.laz";
  VectorReader->open(vector_file);
  std::vector<roofer::LinearRing> polygons;
  VectorReader->readPolygons(polygons);

  roofer::vec1f ground_elevations;
  PointCloudCropper->process(
    las_source,
    polygons,
    polygons,
    point_clouds,
    ground_elevations
  );

  for (auto& pc : point_clouds) {
    std::cout << pc.size() << std::endl;
  }

  // Initialize polyscope
  polyscope::options::programName = "Roofer";
  polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::ShadowOnly;

  polyscope::view::upDir = polyscope::UpDir::ZUp;
  // polyscope::view::style = polyscope::NavigateStyle::Free;

  polyscope::init();

  // Specify the callback
  polyscope::state::userCallback = myCallback;

  // // Register a point cloud
  // // `points` is a Nx3 array-like container of points

    // ++i;
  // }

  // // Register a surface mesh structure
  // // `meshVerts` is a Vx3 array-like container of vertex positions
  // // `meshFaces` is a Fx3 array-like container of face indices  
  // polyscope::registerSurfaceMesh("my mesh", meshVerts, meshFaces);

  // // Add a scalar and a vector function defined on the mesh
  // // `scalarQuantity` is a length V array-like container of values
  // // `vectorQuantity` is an Fx3 array-like container of vectors per face
  // polyscope::getSurfaceMesh("my mesh")->addVertexScalarQuantity("my_scalar", scalarQuantity);
  // polyscope::getSurfaceMesh("my mesh")->addFaceVectorQuantity("my_vector", vectorQuantity);

  // // View the point cloud and mesh we just registered in the 3D UI
  polyscope::show();
}