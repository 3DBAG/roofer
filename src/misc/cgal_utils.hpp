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
#include <reconstruction/MeshTriangulator.hpp>

namespace roofer {

template <typename Point>
CGAL::Surface_mesh<Point> Mesh2CGALSurfaceMesh(const roofer::Mesh& gfmesh) {
  typedef typename CGAL::Surface_mesh<Point> SurfaceMesh;
  typedef typename SurfaceMesh::Vertex_index VertexIndex;
  typedef typename Point::FT FT;
  namespace PMP = CGAL::Polygon_mesh_processing;

  SurfaceMesh smesh;
  typename std::vector<Point> points;

    // method of triangulating everything
    /*
    auto mesh_triangulator = detection::createMeshTriangulatorLegacy();
    std::vector<Mesh> temp_mesh; temp_mesh.push_back(gfmesh);
    mesh_triangulator->compute(temp_mesh);

    std::vector<std::vector<std::size_t>> polygons;
    for (const auto &tri : mesh_triangulator->triangles)
    {
        std::vector<std::size_t> rindices; rindices.reserve(3);
        for (auto &v : tri)
        {
            points.push_back(CGAL::make_array<typename Point::FT>(v[0], v[1], v[2]));
            rindices.push_back(points.size() - 1);
        }
        polygons.push_back(rindices);
    }
     */

    // method of triangulating polygons that have holes
    std::vector<std::vector<std::size_t>> polygons;
    for (const auto &ring : gfmesh.get_polygons())
    {
        if (ring.interior_rings().empty()) {
            std::vector<std::size_t> rindices;
            rindices.reserve(ring.vertex_count());
            for (auto& v: ring) {
                points.push_back(Point(v[0], v[1], v[2]));
                rindices.push_back(points.size() - 1);
            }
            polygons.push_back(rindices);
        } else {
            auto poly_triangulator = detection::createMeshTriangulatorLegacy();
            std::vector<LinearRing> temp_ring; temp_ring.push_back(ring);
            poly_triangulator->compute(temp_ring);
            for (const auto &tri : poly_triangulator->triangles)
            {
                std::vector<std::size_t> rindices; rindices.reserve(3);
                for (auto &v : tri)
                {
                    points.push_back(Point(v[0], v[1], v[2]));
                    rindices.push_back(points.size() - 1);
                }
                polygons.push_back(rindices);
            }
        }
    }


  // turn polygon soup into polygon mesh
  PMP::repair_polygon_soup(points, polygons);
  PMP::orient_polygon_soup(points, polygons);
  PMP::duplicate_non_manifold_edges_in_polygon_soup(points, polygons);
  PMP::polygon_soup_to_polygon_mesh(points, polygons, smesh);

  if(!CGAL::is_triangle_mesh(smesh)) PMP::triangulate_faces(smesh);

  PMP::duplicate_non_manifold_vertices(smesh);

  return smesh;
}

} // namespace roofer
