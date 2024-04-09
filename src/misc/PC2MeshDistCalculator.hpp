#include "../datastructures.hpp"

#include "../reconstruction/cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct PC2MeshDistCalculatorConfig{
  };

  struct PC2MeshDistCalculatorInterface {
    float rms_error;
    vec1f point_errors;
    vec1f face_errors;
    vec1f mesh_error;
    
    // add_output("error_hist", typeid(std::string));
    // add_output("m2pc_error_hist", typeid(std::string));
    // add_output("m2pc_error_max", typeid(float));

    virtual ~PC2MeshDistCalculatorInterface() = default;
    virtual void compute(
      const IndexedPlanesWithPoints& points,
      const MultiTriangleCollection& triangles,
      const roofer::vec1i& face_ids,
      PC2MeshDistCalculatorConfig config=PC2MeshDistCalculatorConfig()
    ) = 0;
    virtual void compute(
      const PointCollection& points,
      const MultiTriangleCollection& triangles,
      const roofer::vec1i& face_ids,
      PC2MeshDistCalculatorConfig config=PC2MeshDistCalculatorConfig()
    ) = 0;
    
  };

  std::unique_ptr<PC2MeshDistCalculatorInterface> createPC2MeshDistCalculator();
}