#pragma once

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Arrangement_2.h>
#include <CGAL/Arr_linear_traits_2.h>
#include <CGAL/Arr_extended_dcel.h>

namespace roofer {

typedef CGAL::Exact_predicates_exact_constructions_kernel EPECK;
typedef CGAL::Exact_predicates_inexact_constructions_kernel EPICK;
typedef EPICK::Point_3 Point;
typedef EPICK::Vector_3 Vector;
typedef EPICK::Vector_2 Vector_2;
typedef EPICK::Plane_3 Plane;
typedef EPICK::Line_3 Line;

typedef std::unordered_map<int, std::pair<Plane, std::vector<Point>>> IndexedPlanesWithPoints;

// Arrangment definitions

typedef CGAL::Arr_linear_traits_2<EPECK>              Traits_2;
typedef Traits_2::Segment_2                           Segment_2;
typedef Traits_2::Point_2                             Point_2;

struct FaceInfo {
  bool is_finite=false;
  bool is_ground=false;
  bool in_footprint=false;
  bool is_footprint_hole=false;
  float elevation_50p=0;
  float elevation_70p=0;
  float elevation_97p=0;
  float elevation_min=0, elevation_max=0;
  float data_coverage=0;
  int pixel_count=0;
  int segid=0;
  int part_id=-1;
  float rms_error_to_avg=0;

  Plane plane;
  std::vector<Point> points;


  // graph-cut optimisation
  size_t label=0;
  size_t v_index;
  std::vector<double> vertex_label_cost;
};
struct EdgeInfo {
  bool blocks = false;
  
  // graph-cut optimisation
  double edge_weight;
};
typedef CGAL::Arr_extended_dcel<Traits_2, bool, EdgeInfo, FaceInfo>   Dcel;
typedef CGAL::Arrangement_2<Traits_2, Dcel>           Arrangement_2;

} // namespace roofer