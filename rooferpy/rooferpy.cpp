// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ivan Paden

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <roofer/roofer.h>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>

using Kernel   = CGAL::Exact_predicates_inexact_constructions_kernel;
using Point2   = CGAL::Point_2<Kernel>;
using Polygon  = CGAL::Polygon_2<Kernel>;
using Pwh      = CGAL::Polygon_with_holes_2<Kernel>;

namespace py = pybind11;

typedef std::vector<std::array<float, 3>> PyPointCollection;
typedef std::vector<std::vector<std::array<float, 3>>> PyLinearRing;
typedef std::vector<PyLinearRing> PyMesh;
typedef std::vector<std::array<size_t, 3>> PyFaceCollection;

namespace roofer {

  // Remove duplicate closing vertex, collapse consecutive duplicates,
  // and project to XY (Z is ignored for footprint).
  Polygon make_poly_2d(const std::vector<std::array<float,3>>& ring) {
    if (ring.size() < 3) return Polygon();

    auto is_same_xy = [](const std::array<float,3>& a, const std::array<float,3>& b){
        return a[0]==b[0] && a[1]==b[1];
    };

    // Strip duplicate closing point
    size_t n = ring.size();
    bool closed = is_same_xy(ring.front(), ring.back());
    size_t m = n - (closed ? 1 : 0);

    Polygon poly;
    poly.reserve(m);

    // Collapse consecutive duplicates
    for (size_t i = 0; i < m; ++i) {
        if (i>0 && is_same_xy(ring[i-1], ring[i])) continue;
        const auto &p = ring[i];
        if (!std::isfinite(p[0]) || !std::isfinite(p[1]))
            throw std::runtime_error("Non-finite XY in footprint");
        poly.push_back(Point2(p[0], p[1]));
    }
    // Ensure at least a triangle
    if (poly.size() < 3) return Polygon();
    return poly;
  }

  // Orient outer CCW and holes CW
  void normalize_orientation(Polygon& outer, std::vector<Polygon>& holes) {
      if (outer.is_clockwise_oriented()) outer.reverse_orientation();
      for (auto& h : holes) {
          if (h.is_counterclockwise_oriented()) h.reverse_orientation();
      }
  }

  // Basic validity checks to fail early (instead of SIGFPE deeper in core)
  // Tiny epsilon for "zero area" checks (units = your footprint units^2)
  inline double area_eps() { return 1e-9; }

  void validate_polygon_with_holes(const Polygon& outer,
                                          const std::vector<Polygon>& holes) {
    if (outer.size() < 3)
        throw std::runtime_error("Outer ring has < 3 vertices");
    if (!outer.is_simple())
        throw std::runtime_error("Outer ring is not simple");
    if (std::abs(outer.area()) <= area_eps())
        throw std::runtime_error("Outer ring has zero (or near-zero) area");

    for (size_t i = 0; i < holes.size(); ++i) {
        const auto& h = holes[i];
        if (h.size() < 3)
            throw std::runtime_error("Hole has < 3 vertices");
        if (!h.is_simple())
            throw std::runtime_error("Hole is not simple");
        if (std::abs(h.area()) <= area_eps())
            throw std::runtime_error("Hole has zero (or near-zero) area");

        // Ensure each hole is strictly inside the outer boundary.
        // (We call this after normalize_orientation, but orientation doesn't matter here.)
        for (auto vit = h.vertices_begin(); vit != h.vertices_end(); ++vit) {
            auto side = outer.bounded_side(*vit);  // if this ever fails to compile, see note below
            if (side != CGAL::ON_BOUNDED_SIDE) {
                throw std::runtime_error("Hole not strictly inside outer boundary");
            }
        }
    }
  }

  // Build Polygon_with_holes from Python-style [[ring0],[ring1],...]
  Pwh build_pwh(const PyLinearRing& footprint) {
      Polygon outer = make_poly_2d(footprint.front());
      std::vector<Polygon> holes;
      holes.reserve(footprint.size() - 1);
      for (size_t i = 1; i < footprint.size(); ++i) {
          Polygon h = make_poly_2d(footprint[i]);
          if (h.size() >= 3) holes.emplace_back(std::move(h));
      }
      normalize_orientation(outer, holes);
      validate_polygon_with_holes(outer, holes);
      return Pwh(outer, holes.begin(), holes.end());
  }

  std::vector<PyMesh> convert_meshes_to_py_meshes(
      const std::vector<Mesh>& meshes) {
    std::vector<PyMesh> py_meshes;
    for (const auto& roofer_mesh : meshes) {
      PyMesh py_mesh;
      for (const auto& roofer_poly : roofer_mesh.get_polygons()) {
        PyLinearRing py_poly;
        std::vector<std::array<float, 3>> py_ring;
        py_ring.reserve(roofer_poly.size());
        for (const auto& pt : roofer_poly) {
          py_ring.push_back({pt[0], pt[1], pt[2]});
        }
        py_poly.push_back(py_ring);
        for (const auto& roofer_interior_ring : roofer_poly.interior_rings()) {
          std::vector<std::array<float, 3>> py_interior_ring;
          py_interior_ring.reserve(roofer_interior_ring.size());
          for (const auto& pt : roofer_interior_ring) {
            py_interior_ring.push_back({pt[0], pt[1], pt[2]});
          }
          py_poly.push_back(py_interior_ring);
        }
        py_mesh.push_back(py_poly);
      }
      py_meshes.push_back(py_mesh);
    }
    return py_meshes;
  }

  Mesh convert_py_mesh_to_mesh(const PyMesh& py_mesh) {
    std::vector<Mesh> meshes;
    Mesh mesh;
    for (const auto& py_poly : py_mesh) {
      LinearRing poly;
      for (const auto& pt : py_poly.front()) {
        poly.push_back({pt[0], pt[1], pt[2]});
      }
      for (size_t i = 1; i < py_poly.size(); ++i) {
        LinearRing iring;
        for (const auto& pt : py_poly[i]) {
          iring.push_back({pt[0], pt[1], pt[2]});
        }
        poly.interior_rings().push_back(iring);
      }
      mesh.push_polygon(poly, 0);
    }
    return mesh;
  }

  std::vector<PyMesh> py_reconstruct(
    const PyPointCollection& points_roof,
    const PyPointCollection& points_ground, 
    const PyLinearRing& footprint,
    ReconstructionConfig cfg) {
      PointCollection pr, pg;
      pr.reserve(points_roof.size());
      pg.reserve(points_ground.size());
      for (const auto& pt : points_roof)  pr.push_back({pt[0], pt[1], pt[2]});
      for (const auto& pt : points_ground) pg.push_back({pt[0], pt[1], pt[2]});

      std::vector<::roofer::Mesh> meshes;

      if (footprint.size() <= 1) {
          LinearRing lr;
          lr.reserve(footprint.front().size());
          for (const auto& p : footprint.front()) lr.push_back({p[0], p[1], p[2]});

          auto same_xy = [](const auto& a, const auto& b){ return a[0]==b[0] && a[1]==b[1]; };
          if (lr.size() >= 2 && same_xy(lr.front(), lr.back())) lr.pop_back();

          meshes = reconstruct(pr, pg, lr, cfg);
      } else {
          Pwh pwh = build_pwh(footprint);
          meshes = reconstruct(pr, pg, pwh, cfg);
      }
      return convert_meshes_to_py_meshes(meshes);
  }

  std::vector<PyMesh> py_reconstruct(
    const PyPointCollection& points_roof,
    const PyLinearRing& footprint,
    ReconstructionConfig cfg) {
      PointCollection pr;
      pr.reserve(points_roof.size());
      for (const auto& pt : points_roof) pr.push_back({pt[0], pt[1], pt[2]});

      if (footprint.empty() || footprint.front().size() < 3) {
          throw std::runtime_error("Footprint outer ring is empty or < 3 vertices");
      }

      std::vector<Mesh> meshes;

      if (footprint.size() <= 1) {
          ::roofer::LinearRing lr;
          lr.reserve(footprint.front().size());
          for (const auto& p : footprint.front()) lr.push_back({p[0], p[1], p[2]});

          auto same_xy = [](const auto& a, const auto& b){ return a[0]==b[0] && a[1]==b[1]; };
          if (lr.size() >= 2 && same_xy(lr.front(), lr.back())) lr.pop_back();

          if (lr.size() < 3) {
              throw std::runtime_error("Footprint outer ring < 3 unique vertices after normalization");
          }

          meshes = reconstruct(pr, lr, cfg);
      } else {
          Pwh pwh = build_pwh(footprint);
          meshes = reconstruct(pr, pwh, cfg);
      }
      return convert_meshes_to_py_meshes(meshes);
  }

  // todo move vertex-face data struct to cpp api?
  std::tuple<PyPointCollection, PyFaceCollection> py_triangulate_mesh(
      const PyMesh& mesh) {
    Mesh roofer_mesh = convert_py_mesh_to_mesh(mesh);
    auto tri_mesh = triangulate_mesh(roofer_mesh);

    std::map<arr3f, size_t> vertex_map;
    PyPointCollection vertices;
    PyFaceCollection faces;
    for (const auto& triangle : tri_mesh) {
      for (const auto& vertex : triangle) {
        if (vertex_map.find(vertex) == vertex_map.end()) {
          vertex_map[vertex] = vertex_map.size();
          vertices.push_back(vertex);
        }
      }
      faces.push_back({vertex_map[triangle[0]], vertex_map[triangle[1]],
                       vertex_map[triangle[2]]});
    }
    return std::make_tuple(vertices, faces);
  }

}  // namespace roofer

PYBIND11_MODULE(roofer, m) {
  py::class_<roofer::ReconstructionConfig>(m, "ReconstructionConfig")
      .def(py::init<>())
      .def_readwrite("complexity_factor",
                     &roofer::ReconstructionConfig::complexity_factor)
      .def_readwrite("clip_ground", &roofer::ReconstructionConfig::clip_ground)
      .def_readwrite("lod", &roofer::ReconstructionConfig::lod)
      .def_readwrite("lod13_step_height",
                     &roofer::ReconstructionConfig::lod13_step_height)
      .def_readwrite("floor_elevation",
                     &roofer::ReconstructionConfig::floor_elevation)
      .def_readwrite(
          "override_with_floor_elevation",
          &roofer::ReconstructionConfig::override_with_floor_elevation)
      .def_readwrite("plane_detect_k",
                     &roofer::ReconstructionConfig::plane_detect_k)
      .def_readwrite("plane_detect_min_points",
                     &roofer::ReconstructionConfig::plane_detect_min_points)
      .def_readwrite("plane_detect_epsilon",
                     &roofer::ReconstructionConfig::plane_detect_epsilon)
      .def_readwrite("plane_detect_normal_angle",
                     &roofer::ReconstructionConfig::plane_detect_normal_angle)
      .def_readwrite("line_detect_epsilon",
                     &roofer::ReconstructionConfig::line_detect_epsilon)
      .def_readwrite("thres_alpha", &roofer::ReconstructionConfig::thres_alpha)
      .def_readwrite("thres_reg_line_dist",
                     &roofer::ReconstructionConfig::thres_reg_line_dist)
      .def_readwrite("thres_reg_line_ext",
                     &roofer::ReconstructionConfig::thres_reg_line_ext)
      .def("is_valid", &roofer::ReconstructionConfig::is_valid);

  m.def("reconstruct",
        py::overload_cast<const PyPointCollection&, const PyPointCollection&,
                          const PyLinearRing&, roofer::ReconstructionConfig>(
            &roofer::py_reconstruct),
        "Reconstruct a single instance of a building from a point cloud with "
        "ground points",
        py::arg("points_roof"), py::arg("points_ground"), py::arg("footprint"),
        py::arg("cfg") = roofer::ReconstructionConfig());

  m.def(
      "reconstruct",
      py::overload_cast<const PyPointCollection&, const PyLinearRing&,
                        roofer::ReconstructionConfig>(&roofer::py_reconstruct),
      "Reconstruct a single instance of a building from a point cloud "
      "without ground points",
      py::arg("points_roof"), py::arg("footprint"),
      py::arg("cfg") = roofer::ReconstructionConfig());

  m.def("triangulate_mesh", &roofer::py_triangulate_mesh, "Triangulate a mesh",
        py::arg("mesh"));
}
