#include "ArrangementBase.hpp"
#include "MeshTriangulator.hpp"
#include "cdt_util.hpp"
#include <CGAL/Cartesian.h>

namespace roofer::detection {

  namespace tri = tri_util;

  typedef CGAL::Cartesian<float> AK;
  typedef CGAL::Polygon_2<tri::K> iPolygon_2;
  typedef CGAL::Plane_3<tri::K> Plane_3;

  AK::Vector_3 calculate_normal(const LinearRing& ring)
  {
    float x=0, y=0, z=0;
    for (size_t i = 0; i < ring.size(); ++i) {
      const auto &curr = ring[i];
      const auto &next = ring[(i + 1) % ring.size()];
      x += (curr[1] - next[1]) * (curr[2] + next[2]);
      y += (curr[2] - next[2]) * (curr[0] + next[0]);
      z += (curr[0] - next[0]) * (curr[1] + next[1]);
    }
    AK::Vector_3 n(x, y, z);
    return n / CGAL::approximate_sqrt(n.squared_length());
  }

  double calculate_volume(const TriangleCollection& triangle_collection) {
    double sum = 0;
    for(const auto& t : triangle_collection) {
      auto a = Vector(t[0][0], t[0][1], t[0][2]);
      auto b = Vector(t[1][0], t[1][1], t[1][2]);
      auto c = Vector(t[2][0], t[2][1], t[2][2]);
      sum += CGAL::scalar_product(a, CGAL::cross_product(b, c));
    }
    return sum/6;
  }

  iPolygon_2 project(roofer::vec3f& ring, Plane_3& plane) {
    iPolygon_2 poly_2d;
    for (auto& p : ring) {
      poly_2d.push_back(plane.to_2d(tri::K::Point_3(p[0], p[1], p[2])));
    }
    return poly_2d;
  }

  void project_and_insert(roofer::vec3f& ring, Plane_3& plane, tri::CDT& cdt) {
    auto pit_last = ring.end()-1;
    tri::CDT::Vertex_handle vh_next, vh_last, vh = cdt.insert(plane.to_2d(tri::K::Point_3((*pit_last)[0], (*pit_last)[1], (*pit_last)[2])));
    vh_last = vh;
    vh->info().set_point(*pit_last);
    for (auto pit = ring.begin(); pit != ring.end(); ++pit) {
      if(pit==pit_last){
        vh_next=vh_last;
      } else {
        vh_next = cdt.insert(plane.to_2d(tri::K::Point_3((*pit)[0], (*pit)[1], (*pit)[2])));
        vh_next->info().set_point(*pit);
      }
      cdt.insert_constraint(vh, vh_next);
      vh = vh_next;
    }
  }
  class MeshTriangulatorLegacy : public MeshTriangulatorInterface{

    void triangulate_polygon(
      const LinearRing& poly_,
      vec3f& normals,
      TriangleCollection& triangles,
      size_t& ring_id,
      vec1i& ring_ids,
      const MeshTriangulatorConfig& cfg
    ) {
      LinearRing poly = poly_;
      float dupe_threshold = (float) std::pow(10,-cfg.dupe_threshold_exp);
      if (is_degenerate(poly, dupe_threshold)) {
        LinearRing new_poly = fix_duplicates(poly, dupe_threshold);
        if(is_degenerate(new_poly, dupe_threshold)) {
          std::cout << "skipping ring with duplicates\n";
          // dupe_rings.push_back(poly);
          return;
        }
        std::cout << "fixed ring with duplicates\n";
        poly = new_poly;
      }
      auto normal = calculate_normal(poly);
      if (std::isnan(normal.x()) || std::isnan(normal.y()) || std::isnan(normal.z())){
        std::cout << "degenerate normal: " << normal[0] << " " << normal[1] << " " << normal[2] << "\n";
        return;
      }
      auto& p0 = poly[0];
      Plane_3 plane(tri::K::Point_3(p0[0], p0[1], p0[2]), tri::K::Vector_3(normal.x(), normal.y(), normal.z()));

      // project and triangulate
      tri::CDT triangulation;
      // Polygon_2 poly_2d = project(poly, plane);
      // if(CGAL::abs(poly_2d.area())<1E-4) {
      //   return;
      // }
      project_and_insert(poly, plane, triangulation);
      // triangulation.insert_constraint(poly_2d.vertices_begin(), poly_2d.vertices_end(), true);
      for (auto& ring : poly.interior_rings()) {
        project_and_insert(ring, plane, triangulation);
        // poly_2d = project(poly, plane);
        // triangulation.insert_constraint(poly_2d.vertices_begin(), poly_2d.vertices_end(), true);
      }

      if (triangulation.number_of_faces()==0)
        return;

      mark_domains(triangulation);

      // for (auto& e : triangulation.finite_edges()) {
      //   auto source = e.first->vertex(triangulation.cw(e.second))->info().point;
      //   auto target = e.first->vertex(triangulation.ccw(e.second))->info().point;
        // edges.push_back({
        //   arr3f{source},
        //   arr3f{target}
        // });
        // bool constr = triangulation.is_constrained(e);
        // edges_constr.push_back(constr);
        // edges_constr.push_back(constr);
      // }

      for (tri::CDT::Finite_faces_iterator fit = triangulation.finite_faces_begin();
      fit != triangulation.finite_faces_end(); ++fit) {

        if (!cfg.output_all_triangles && !fit->info().in_domain()) continue;

        Triangle triangle;
        triangle = {
          fit->vertex(0)->info().point,
          fit->vertex(1)->info().point,
          fit->vertex(2)->info().point
        };
        for (size_t j = 0; j < 3; ++j)
        {
          normals.push_back({normal.x(), normal.y(), normal.z()});
          // values_out.push_back(values_in[vi]);
          ring_ids.push_back(ring_id);
          // nesting_levels.push_back(fit->info().nesting_level);
        }
        triangles.push_back(triangle);
      }
    }

    public:

    void compute(
      const std::vector<Mesh>& meshes,
      MeshTriangulatorConfig cfg
    ) override {
      typedef uint32_t N;

      roofer::MultiTriangleCollection multitranglecol;

      for (size_t mi = 0; mi < meshes.size(); ++mi) {
        auto mesh = meshes[mi];
        TriangleCollection mesh_triangles;
        AttributeMap mesh_attributes;
        std::vector<attribute_value> tri_labels;
        for (size_t ri = 0; ri<mesh.get_polygons().size(); ++ri) {
          TriangleCollection tc;
          triangulate_polygon(mesh.get_polygons()[ri], normals, tc, ri, ring_ids, cfg);
          int poly_label = mesh.get_labels()[ri];
          // Need to get a label for each triangle that was generated
          for (size_t i = 0; i<tc.size(); i++) tri_labels.emplace_back(poly_label);
          triangles.insert(triangles.end(), tc.begin(), tc.end());
          mesh_triangles.insert(mesh_triangles.end(), tc.begin(), tc.end());
        }
        volumes.push_back((float)calculate_volume(mesh_triangles));
        mesh_attributes["labels"] = tri_labels;
        multitrianglecol.push_back(mesh_triangles);
        multitrianglecol.push_back(mesh_attributes);
        multitrianglecol.building_part_ids_.push_back(mi);
      }

    }
    void compute(
      const std::vector<LinearRing>& polygons,
      MeshTriangulatorConfig cfg
    ) override {
      // const auto &values_in = input("valuesf").get<vec1f>();
      typedef uint32_t N;

      for (size_t ri = 0; ri < polygons.size(); ++ri)
      {
        TriangleCollection tc;
        triangulate_polygon(polygons[ri], normals, tc, ri, ring_ids, cfg);
        triangles.insert(triangles.end(), tc.begin(), tc.end());
      }
      volumes.push_back((float)calculate_volume(triangles));
      multitrianglecol.push_back(triangles);
    }

    void compute(
      const std::unordered_map<int, Mesh>& multisolid,
      MeshTriangulatorConfig cfg
    ) override {
      // const auto &values_in = input("valuesf").get<vec1f>();
      typedef uint32_t N;

      // We are processing a building part here. We get a building part when we
      // cut a footprint into parts because of cutting off the underground part.
      double volume_sum = 0;
      for(auto& [sid, mesh] : multisolid) {
        TriangleCollection mesh_triangles;
        AttributeMap mesh_attributes;
        std::vector<attribute_value> tri_labels;
        for (size_t ri = 0; ri<mesh.get_polygons().size(); ++ri) {
          TriangleCollection tc;
          triangulate_polygon(mesh.get_polygons()[ri], normals, tc, ri, ring_ids, cfg);
          int poly_label = mesh.get_labels()[ri];
          // Need to get a label for each triangle that was generated
          for (size_t i = 0; i<tc.size(); i++) tri_labels.emplace_back(poly_label);
          triangles.insert(triangles.end(), tc.begin(), tc.end());
          mesh_triangles.insert(mesh_triangles.end(), tc.begin(), tc.end());
        }
        mesh_attributes["labels"] = tri_labels;
        multitrianglecol.push_back(mesh_triangles);
        multitrianglecol.push_back(mesh_attributes);
        multitrianglecol.building_part_ids_.push_back(sid);
        volume_sum += calculate_volume(mesh_triangles);
      }
      volumes.push_back((float)volume_sum);
    }

    // void MTC2MMNode::process()
    // {
    //   auto& mtc = input("multi_triangle_collections").get<MultiTriangleCollection&>();
    //   std::unordered_map<int, Mesh> meshmap;

    //   auto& attrmap = mtc.get_attributes();
    //   size_t i=0;
    //   for(auto& tc : mtc.get_tricollections()) {
    //     Mesh mesh;
    //     size_t j=0;
    //     for (auto& triangle : tc) {
    //       LinearRing lr;
    //       lr.push_back(triangle[0]);
    //       lr.push_back(triangle[1]);
    //       lr.push_back(triangle[2]);
    //       mesh.push_polygon( lr, std::get<int>( attrmap[i]["labels"][j++] ) );
    //     }

    //     meshmap[mtc.building_part_ids_[i++]] = mesh;
    //   }
    //   output("meshmap").set(meshmap);

    // }
  };

  std::unique_ptr<MeshTriangulatorInterface> createMeshTriangulatorLegacy() {
    return std::make_unique<MeshTriangulatorLegacy>();
  }

} // namespace roofer::detection