// This file is part of gfp-building-reconstruction
// Copyright (C) 2018-2022 Ravi Peters

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arr_observer.h>
#include <CGAL/Arr_overlay_2.h>
#include <CGAL/Arr_default_overlay_traits.h>
#include <CGAL/Polygon_2.h>

#include "cgal_shared_definitions.hpp"
#include "../datastructures.hpp"

namespace roofer::detection {

typedef CGAL::Polygon_2<EPECK>                           Polygon_2;

// 2D arrangement
// typedef CGAL::Cartesian<Number_type>           AK;
typedef Traits_2::Ray_2                               Ray_2;
typedef Traits_2::Line_2                              Line_2;
typedef Traits_2::X_monotone_curve_2                  X_monotone_curve_2;


typedef Arrangement_2::Vertex_handle                  Vertex_handle;
typedef Arrangement_2::Halfedge_handle                Halfedge_handle;
typedef Arrangement_2::Face_handle                    Face_handle;
typedef Arrangement_2::Vertex_const_handle            Vertex_const_handle;
typedef Arrangement_2::Halfedge_const_handle          Halfedge_const_handle;
typedef Arrangement_2::Face_const_handle              Face_const_handle;
typedef Arrangement_2::Ccb_halfedge_circulator        Ccb_halfedge_circulator;
typedef CGAL::Arr_accessor<Arrangement_2>             Arr_accessor;
typedef Arr_accessor::Dcel_vertex                     DVertex;
typedef Arrangement_2::Face                           Face;
struct overlay_functor {
  FaceInfo operator()(const FaceInfo a, const FaceInfo b) const {
    auto r = FaceInfo();
    r.segid=0;
    // if (a.is_finite && b.is_finite)
    r.is_finite = true;

    if (a.segid!=0 && b.segid==0) {
      r = a;
    } else if (a.segid==0 && b.segid!=0) {
      r = b;
    } else if (a.segid!=0 && b.segid!=0) { // we need to merge 2 faces with a plane
      if (a.elevation_70p > b.elevation_70p) {
        r=a;
      } else {
        r=b;
      }
    }

    if (a.in_footprint || b.in_footprint) {
      r.in_footprint = true;
    }

    return r;
  }
};
typedef CGAL::Arr_face_overlay_traits<Arrangement_2,
                                      Arrangement_2,
                                      Arrangement_2,
                                      overlay_functor >  Overlay_traits;
// typedef CGAL::Cartesian_converter<IK,EK>                         IK_to_EK;
// typedef CGAL::Cartesian_converter<Number_type,SCK>          ToInexact;

// inline Number_type arr_s(double v){return static_cast<Number_type>(100*v);}

// An arrangement observer, used to receive notifications of face splits and
// to update the indices of the newly created faces.
class Face_index_observer : public CGAL::Arr_observer<Arrangement_2>
{
private:
  int     n_faces;          // The current number of faces.
  size_t  plane_id;
  bool    in_footprint;
  float   elevation=0;
  Plane   plane;
public:
  Face_index_observer (Arrangement_2& arr, bool is_footprint, size_t pid, float elevation, Plane plane) :
    CGAL::Arr_observer<Arrangement_2> (arr),
    n_faces (0), in_footprint(is_footprint), plane_id(pid), elevation(elevation), plane(plane)
  {
    CGAL_precondition (arr.is_empty());
    arr.unbounded_face()->data().is_finite=false;
    n_faces++;
  };
  virtual void after_split_face (Face_handle old_face,
                                 Face_handle new_face, bool )
  {
    // Assign index to the new face.
    new_face->data().in_footprint = in_footprint;
    new_face->data().is_finite = true;
    new_face->data().segid = plane_id;
    new_face->data().elevation_70p = elevation;
    new_face->data().plane = plane;
    n_faces++;
  }
};
class Face_split_observer : public CGAL::Arr_observer<Arrangement_2>
{
private:
  int   n_faces;          // The current number of faces.
  bool  hole_mode=false;
public:
  Face_split_observer (Arrangement_2& arr) :
    CGAL::Arr_observer<Arrangement_2> (arr),
    n_faces (0)
  {
    CGAL_precondition (arr.is_empty());
    arr.unbounded_face()->data().in_footprint=false;
    n_faces++;
  }
  virtual void after_split_face (Face_handle old_face,
                                 Face_handle new_face, bool is_hole)
  {
    // Assign index to the new face.
    if(n_faces == 1)
      new_face->data().in_footprint = true;
    else if(old_face->data().in_footprint) {
      if (!is_hole && hole_mode) { // detect case where a `hole` is added that touches surrounding outer_ccb (in one vertex)
        // these holes are ignored
        new_face->data().in_footprint = true;
        new_face->data().is_footprint_hole = false;
        old_face->data().in_footprint = true;
        old_face->data().is_footprint_hole = false;
        std::cout << "Ignored input footprint hole that is touching footprint exterior\n";
      } else { // normal holes that do not touch outer_ccb of existing face
        new_face->data().in_footprint = !hole_mode;
        new_face->data().is_footprint_hole = hole_mode;
      }
    } else {
      new_face->data().in_footprint = false;
      new_face->data().is_footprint_hole = old_face->data().is_footprint_hole;
    }
    n_faces++;
  }
  void set_hole_mode(bool mode) {
    hole_mode = mode;
  }
};
class Face_merge_observer : public CGAL::Arr_observer<Arrangement_2>
{
  public:
  Face_merge_observer (Arrangement_2& arr) :
    CGAL::Arr_observer<Arrangement_2> (arr) {};

  virtual void before_merge_face (Face_handle remaining_face,
                                 Face_handle discarded_face, Halfedge_handle e )
  {
    auto count1 = float(remaining_face->data().pixel_count);
    auto count2 = float(discarded_face->data().pixel_count);
    auto sum_count = count1+count2;
    if (sum_count!=0){
      auto w1 = (count1/sum_count);
      auto w2 = (count2/sum_count);
      remaining_face->data().elevation_50p =
        remaining_face->data().elevation_50p * w1 + discarded_face->data().elevation_50p * w2;
      remaining_face->data().elevation_70p =
        remaining_face->data().elevation_70p * w1 + discarded_face->data().elevation_70p * w2;
      remaining_face->data().elevation_97p =
        std::max(remaining_face->data().elevation_97p, discarded_face->data().elevation_97p);
      remaining_face->data().data_coverage =
        remaining_face->data().data_coverage * w1 + discarded_face->data().data_coverage * w2;
      // and sum the counts
      remaining_face->data().pixel_count = sum_count;
    }
    remaining_face->data().elevation_min = std::min(remaining_face->data().elevation_min, discarded_face->data().elevation_min);
    remaining_face->data().elevation_max = std::max(remaining_face->data().elevation_max, discarded_face->data().elevation_max);
    // merge the point lists
    // if (remaining_face==discarded_face){
    //   std::cout << "merging the same face!?\n";
    //   return;
    // }
    // remaining_face->data().points.insert(remaining_face->data().points.end(), discarded_face->data().points.begin(), discarded_face->data().points.end() );
  }
};

class Snap_observer : public CGAL::Arr_observer<Arrangement_2>
{
  public:
  Snap_observer (Arrangement_2& arr) :
    CGAL::Arr_observer<Arrangement_2> (arr) {};

  virtual void 	after_create_edge (Halfedge_handle e) {
    e->data().blocks = true;
  }
  virtual void after_split_face (Face_handle old_face, Face_handle new_face, bool ) {
    if (old_face->data().segid==0)
      new_face->set_data(old_face->data());
  }
};

typedef std::vector<std::array<float,2>> vec2f;

void arr_filter_biggest_face(Arrangement_2& arr, const float& rel_area_thres);

void arrangementface_to_polygon(Face_handle face, vec2f& polygons);
bool arrangementface_to_polygon(Face_handle face, roofer::LinearRing& polygons, double h=0);

Polygon_2 arr_cell2polygon(const Face_handle& fh);

void arr_dissolve_seg_edges(Arrangement_2& arr);
void arr_remove_redundant_vertices(Arrangement_2& arr);
void arr_dissolve_step_edges_naive(Arrangement_2& arr, float step_height_threshold, bool compute_on_edge);
void arr_dissolve_step_edges(Arrangement_2& arr, float step_height_threshold);
void arr_dissolve_fp(Arrangement_2& arr, bool inside, bool outside);
void arr_snap_duplicates(Arrangement_2& arr, double dupe_threshold);
void arr_label_buildingparts(Arrangement_2& arr);

} // namespace roofer