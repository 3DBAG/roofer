#if defined (_MSC_VER) && !defined (_WIN64)
#pragma warning(disable:4244) // boost::number_distance::distance()
                              // converts 64 to 32 bits integers
#endif
#include "ShapeDetector.hpp"
#include <iostream>
#include <utility>
#include <CGAL/property_map.h>
#include <CGAL/IO/read_points.h>
#include <CGAL/Point_with_normal_3.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Shape_detection/Efficient_RANSAC.h>
#include <CGAL/pca_estimate_normals.h>
#include <CGAL/tags.h>

namespace roofer::detection {

struct PlaneDetectorRANSAC : public ShapeDetectorInterface {
  int metrics_normal_k = 5;

  // Type declarations.
  typedef CGAL::Exact_predicates_inexact_constructions_kernel  Kernel;
  typedef std::pair<Kernel::Point_3, Kernel::Vector_3>         Point_with_normal;
  typedef std::vector<Point_with_normal>                       Pwn_vector;
  typedef CGAL::First_of_pair_property_map<Point_with_normal>  Point_map;
  typedef CGAL::Second_of_pair_property_map<Point_with_normal> Normal_map;
  typedef CGAL::Shape_detection::Efficient_RANSAC_traits
  <Kernel, Pwn_vector, Point_map, Normal_map>             Traits;
  typedef CGAL::Shape_detection::Efficient_RANSAC<Traits> Efficient_ransac;
  typedef CGAL::Shape_detection::Plane<Traits>            Plane;

  unsigned detectPlanes (
    PointCollection& point_collection, 
    vec3f& normals, 
    vec1i& labels,
    float probability,
    int min_points,
    float epsilon,
    float cluster_epsilon,
    float normal_threshold
  ) override {
    std::cout << "Efficient RANSAC" << std::endl;

    // Points with normals.
    Pwn_vector points;

    for (auto& p : point_collection) {
      Point_with_normal pn;
      std::get<0>(pn) = Kernel::Point_3(p[0], p[1], p[2]);
      points.push_back(pn);
    }

    // estimate normals
    if (points.size()) {
      CGAL::pca_estimate_normals<CGAL::Sequential_tag>(
        points, metrics_normal_k,
        CGAL::parameters::point_map(Point_map()).
        normal_map(Normal_map())
      );
    }
    // orient normals upwards
    auto up = Kernel::Vector_3(0,0,1);
    for ( auto& pv : points) {
      auto &n = std::get<1>(pv);
      if (n*up<0) 
        std::get<1>(pv) = -n;
    }

    // Instantiate shape detection engine.
    Efficient_ransac ransac;
    // Provide input data.
    ransac.set_input(points);
    // Register planar shapes via template method.
    ransac.add_shape_factory<Plane>();

    // Set parameters for shape detection.
    Efficient_ransac::Parameters parameters;
    // Set probability to miss the largest primitive at each iteration.
    parameters.probability = probability;
    // Detect shapes with at least 200 points.
    parameters.min_points = min_points;
    // Set maximum Euclidean distance between a point and a shape.
    parameters.epsilon = epsilon;
    // Set maximum Euclidean distance between points to be clustered.
    parameters.cluster_epsilon = cluster_epsilon;
    // Set maximum normal deviation.
    // 0.9 < dot(surface_normal, point_normal);
    parameters.normal_threshold = normal_threshold;
    // Detect shapes.
    ransac.detect(parameters);
    // Print number of detected shapes.
    std::cout << ransac.shapes().end() - ransac.shapes().begin()
    << " shapes detected." << std::endl;
    
    labels.resize(point_collection.size());
    unsigned shape_id = 1;
    for(auto shape: ransac.shapes()){
      // auto& plane = *dynamic_cast<Plane_traits*>(shape.get());
      // Plane plane_cgal(boost::get<0>(pnl_points[plane.indices_of_assigned_points().front()]), plane.plane_normal());
      for (const size_t& i : shape->indices_of_assigned_points()) {
        labels[i] = shape_id;
      }
      ++shape_id;
    }

    // RANSAC detect() reorders the data!
    point_collection.clear();
    point_collection.reserve(points.size());
    normals.reserve(points.size());
    for (auto& pt : points) {
      auto& p = std::get<0>(pt);
      auto& n = std::get<1>(pt);
      point_collection.push_back(
        {float(CGAL::to_double(p.x())), float(CGAL::to_double(p.y())), float(CGAL::to_double(p.z()))}
      );
      normals.push_back(
        {float(CGAL::to_double(n.x())), float(CGAL::to_double(n.y())), float(CGAL::to_double(n.z()))}
      );
    }
    return ransac.shapes().end() - ransac.shapes().begin();
  }
};

std::unique_ptr<ShapeDetectorInterface> createShapeDetector() {
  return std::make_unique<ShapeDetector>();
};

}