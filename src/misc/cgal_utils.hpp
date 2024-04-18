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

#include "datastructures.hpp"

#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup_extension.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>

namespace roofer {

template <typename Point>
CGAL::Surface_mesh<Point> Mesh2CGALSurfaceMesh(const roofer::Mesh& gfmesh) {
  typedef typename CGAL::Surface_mesh<Point> SurfaceMesh;
  typedef typename SurfaceMesh::Vertex_index VertexIndex;
  namespace PMP = CGAL::Polygon_mesh_processing;

  SurfaceMesh smesh;
  std::map<arr3f, std::size_t> vertex_map;
  std::set<arr3f> vertex_set;
  std::vector<Point> points;
  for (const auto &ring : gfmesh.get_polygons())
  {
    for (auto &v : ring)
    {
      auto [it, did_insert] = vertex_set.insert(v);
      if (did_insert)
      {
        vertex_map[v] = points.size();
        points.push_back(Point(v[0],v[1],v[2]));
      }
    }
  }

  // First build a polygon soup
  std::vector<std::vector<std::size_t> > polygons;
  for (auto& ring : gfmesh.get_polygons()) {
    std::vector<std::size_t> rindices;
    rindices.reserve(ring.size());
    for(auto& p : ring) {
      rindices.push_back(vertex_map[p]);
    }
    polygons.push_back(rindices);
  }
  CGAL::Polygon_mesh_processing::repair_polygon_soup(points, polygons);
  CGAL::Polygon_mesh_processing::orient_polygon_soup(points, polygons);
  CGAL::Polygon_mesh_processing::duplicate_non_manifold_edges_in_polygon_soup(points, polygons);
  CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(points, polygons, smesh);

  if(!CGAL::is_triangle_mesh(smesh)) PMP::triangulate_faces(smesh);

  CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices(smesh);

  return smesh;
}

} // namespace roofer
