#pragma once

#include "../detection/cgal_shared_definitions.hpp"

#include <CGAL/Arrangement_2.h>
#include <CGAL/Arr_linear_traits_2.h>
#include <CGAL/Arr_extended_dcel.h>

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