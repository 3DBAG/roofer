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
#include "cdt_util.hpp"

#include <CGAL/Polygon_2.h>
#include <CGAL/Barycentric_coordinates_2/triangle_coordinates_2.h>
//todo temp for testing
#include <CGAL/Surface_mesh.h>


namespace roofer {

namespace tri_util {

  void mark_domains(CDT& ct,
    CDT::Face_handle start,
    int index,
    std::list<CDT::Edge>& border) {
    if (start->info().nesting_level != -1) {
      return;
    }
    std::list<CDT::Face_handle> queue;
    queue.push_back(start);
    while (!queue.empty()) {
      CDT::Face_handle fh = queue.front();
      queue.pop_front();
      if (fh->info().nesting_level == -1) {
        fh->info().nesting_level = index;
        for (int i = 0; i < 3; i++) {
          CDT::Edge e(fh, i);
          CDT::Face_handle n = fh->neighbor(i);
          if (n->info().nesting_level == -1) {
            if (ct.is_constrained(e)) border.push_back(e);
            else queue.push_back(n);
          }
        }
      }
    }
  }
  /**
  * mark the triangles that are inside the original 2D polygon.
  * explore set of facets connected with non constrained edges,
  * and attribute to each such set a nesting level.
  * start from facets incident to the infinite vertex, with a nesting
  * level of 0. Then recursively consider the non-explored facets incident
  * to constrained edges bounding the former set and increase the nesting level by 1.
  * facets in the domain are those with an odd nesting level.
  */
  void mark_domains(CDT& cdt) {
    // for (CDT::All_faces_iterator it = cdt.all_faces_begin(); it != cdt.all_faces_end(); ++it) {
    //   it->info().nesting_level = -1;
    // }
    std::list<CDT::Edge> border;
    mark_domains(cdt, cdt.infinite_face(), 0, border);
    while (!border.empty()) {
      CDT::Edge e = border.front();
      border.pop_front();
      CDT::Face_handle n = e.first->neighbor(e.second);
      if (n->info().nesting_level == -1) {
        mark_domains(cdt, n, e.first->info().nesting_level + 1, border);
      }
    }
  }

  void insert_ring(roofer::vec3f& ring, CDT& cdt) {
    auto pit_last = ring.end()-1;
    CDT::Vertex_handle vh_next, vh_last, vh = cdt.insert(K::Point_2((*pit_last)[0], (*pit_last)[1]));
    vh_last = vh;
    vh->info().set_point(*pit_last);
    for (auto pit = ring.begin(); pit != ring.end(); ++pit) {
      if(pit==pit_last){
        vh_next=vh_last;
      } else {
        vh_next = cdt.insert(K::Point_2((*pit)[0], (*pit)[1]));
        vh_next->info().set_point(*pit);
      }
      cdt.insert_constraint(vh, vh_next);
      vh = vh_next;
    }
  }

  CDT create_from_polygon(roofer::LinearRing& poly) {
    CDT triangulation;

    insert_ring(poly, triangulation);
    for (auto& ring : poly.interior_rings()) {
      insert_ring(ring, triangulation);
    }

    if (triangulation.number_of_faces()==0)
      return triangulation;

    mark_domains(triangulation);

    return triangulation;
  }

} // namespace tri_util

namespace proj_tri_util {
  DT cdt_from_linearing(const roofer::LinearRing& poly) {
    // store roofer's LinearRing as CGAL Polygon_2 with proj traits
    typedef CGAL::Polygon_2<Projection_traits> Polygon_3;
    typedef Projection_traits::Point_2 Point_3;
    DT cdt;
    for (auto& p : poly)
      cdt.insert(Point_3(p[0], p[1], p[2]));
    for (auto& ring : poly.interior_rings())
      for (auto& p : ring) cdt.insert(Point_3(p[0], p[1], p[2]));

    return cdt;
  }

  float interpolate_from_cdt(const Point_2& p, const DT& cdt)  {
    DT::Face_handle fh = nullptr;
    DT::Point pt(CGAL::to_double(p.x()), CGAL::to_double(p.y()), 0);

    DT::Locate_type lt;
    int li;
    fh = cdt.locate(pt, lt, li, fh);
    if (lt == DT::OUTSIDE_CONVEX_HULL) {
      // borderline case when point is on the edge of the convex hull
      std::vector<DT::Point> interp_edge;
      // the point landed on infinite face on the other side of the edge
      if (!cdt.is_infinite(fh->vertex(0))) interp_edge.push_back(fh->vertex(0)->point());
      if (!cdt.is_infinite(fh->vertex(1))) interp_edge.push_back(fh->vertex(1)->point());
      if (!cdt.is_infinite(fh->vertex(2))) interp_edge.push_back(fh->vertex(2)->point());
      assert(interp_edge.size() == 2);

      EPICK::Point_2 pt1(interp_edge[0].x(), interp_edge[0].y());
      EPICK::Point_2 pt2(interp_edge[1].x(), interp_edge[1].y());
      EPICK::Point_2 pt_int(pt.x(), pt.y());

      // approximate with 1D linear interpolation along the neighbouring edge
      double g1 = CGAL::approximate_sqrt(CGAL::squared_distance(pt1, pt_int))
                  / CGAL::approximate_sqrt(CGAL::squared_distance(pt1, pt2));

      float h_int = interp_edge[0].z() + g1 * (interp_edge[1].z() - interp_edge[0].z());

      return h_int;
    }

    std::vector<double> coords;
    CGAL::Barycentric_coordinates::triangle_coordinates_2(
        EPICK::Point_2(fh->vertex(0)->point().x(), fh->vertex(0)->point().y()),
        EPICK::Point_2(fh->vertex(1)->point().x(), fh->vertex(1)->point().y()),
        EPICK::Point_2(fh->vertex(2)->point().x(), fh->vertex(2)->point().y()),
        EPICK::Point_2(pt.x(), pt.y()),
        std::back_inserter(coords)
    );

    float h = 0;
    for (int i = 0; i < 3; ++i) {

      h += fh->vertex(i)->point().z() * coords[i];
    }
    return h;
  }

  //todo temp function for testing
  void write_cdt_to_obj(const DT& cdt, const std::string& filename) {
    typedef CGAL::Surface_mesh<EPICK::Point_3> Mesh;
    std::map<DT::Vertex_handle, int> indices;
    std::vector<Mesh::vertex_index> mesh_vertex;
    std::vector<Mesh::face_index> face_index;
    mesh_vertex.reserve(cdt.number_of_vertices());
    int counter = 0;
    Mesh mesh;
    for (const auto& it : cdt.finite_vertex_handles()) {
      mesh_vertex.emplace_back(mesh.add_vertex(it->point()));
      //        outstream << it->point() << std::endl;
      indices.insert(std::pair<DT::Vertex_handle, int>(it, counter++));
    }
    for (const auto& it : cdt.finite_face_handles()) {
      int v1 = indices[it->vertex(0)];
      int v2 = indices[it->vertex(1)];
      int v3 = indices[it->vertex(2)];
      mesh.add_face(mesh_vertex[v1], mesh_vertex[v2], mesh_vertex[v3]);
    }
    CGAL::IO::write_OBJ(filename, mesh);
  }

} // namespace proj_tri_util

} //namespace roofer