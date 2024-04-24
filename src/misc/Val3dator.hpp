#include "../datastructures.hpp"

#include "../reconstruction/cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct Val3datorConfig{
    // bool log_invalids=false;
    float tol_planarity_d2p_ = 0.01;
    float tol_planarity_normals_ = 20;
  };

  struct Val3datorInterface {
    std::vector<std::string> errors;
    std::vector<LinearRing> error_faces;
    PointCollection error_locations;

    virtual ~Val3datorInterface() = default;
    virtual void compute(
      const std::unordered_map<int, Mesh>& mesh,
      Val3datorConfig config=Val3datorConfig()
    ) = 0;
    virtual void compute(
      const TriangleCollection& triangles,
      Val3datorConfig config=Val3datorConfig()
    ) = 0;
    
  };

  std::unique_ptr<Val3datorInterface> createVal3dator();
}