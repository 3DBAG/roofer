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

#include "../datastructures.hpp"
#include "cgal_shared_definitions.hpp"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Projection_traits_xy_3.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>

#include <CGAL/Triangulation_vertex_base_with_id_2.h>

namespace roofer {

namespace tri_util {

  typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
  typedef CGAL::Exact_predicates_tag Tag;
  struct VertexInfo {
    bool hasPoint = false;
    roofer::arr3f point;
    VertexInfo() : hasPoint(){};
    void set_point(roofer::arr3f& p) {
      hasPoint=true;
      point = p;
    };
  };
  struct FaceInfo {
    bool interior = false, visited = false;
    int nesting_level = -1;
    void set_interior(bool is_interior) {
      visited = true;
      interior = is_interior;
    }
    bool in_domain() {
      return nesting_level % 2 == 1;
    }
  };
  typedef CGAL::Triangulation_vertex_base_with_info_2<VertexInfo, K> VertexBase;
  typedef CGAL::Constrained_triangulation_face_base_2<K> FaceBase;
  typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo, K, FaceBase> FaceBaseWithInfo;
  typedef CGAL::Triangulation_data_structure_2<VertexBase, FaceBaseWithInfo> TriangulationDataStructure;
  typedef CGAL::Constrained_Delaunay_triangulation_2<K, TriangulationDataStructure, Tag> CDT;

  void mark_domains(CDT& ct,
    CDT::Face_handle start,
    int index,
    std::list<CDT::Edge>& border);

  void mark_domains(CDT& cdt);

  void insert_ring(roofer::vec3f& ring, CDT& cdt);

  CDT create_from_polygon(roofer::LinearRing& poly);

} // namespace tri_util

// utils for triangulation with projection traits
namespace proj_tri_util {
  typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
  typedef CGAL::Projection_traits_xy_3<K> Projection_traits;
  typedef CGAL::Delaunay_triangulation_2<Projection_traits> DT;

  DT cdt_from_linearing(const roofer::LinearRing& poly);

  float interpolate_from_cdt(const Point_2& p, const DT& cdt);

  //todo temp for testing
  void write_cdt_to_obj(const DT& cdt, const std::string& filename);

} // namespace proj_tri_util

} // namespace roofer