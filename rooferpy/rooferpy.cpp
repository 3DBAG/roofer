// Copyright (c) 2018-2026 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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

#include <algorithm>
#include <cctype>
#include <type_traits>
#include <utility>

namespace py = pybind11;

typedef std::vector<std::array<float, 3>> PyPointCollection;
typedef std::vector<std::vector<std::array<float, 3>>> PyLinearRing;
typedef std::vector<PyLinearRing> PyMesh;
typedef std::vector<std::array<size_t, 3>> PyFaceCollection;

namespace roofer {
  void convert_to_linear_ring(const PyLinearRing& footprint,
                              roofer::LinearRing& linear_ring) {
    for (const auto& p : footprint.front()) {
      float x = p[0];
      float y = p[1];
      float z = p[2];
      linear_ring.push_back({x, y, z});
    }
    for (size_t i = 1; i < footprint.size(); ++i) {
      roofer::LinearRing iring;
      for (const auto& p : footprint[i]) {
        float x = p[0];
        float y = p[1];
        float z = p[2];
        iring.push_back({x, y, z});
      }
      linear_ring.interior_rings().push_back(iring);
    }
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
      const PyPointCollection& points_ground, const PyLinearRing& footprint,
      ReconstructOptions cfg = ReconstructOptions()) {
    PointCollection points_roof_pc, points_ground_pc;
    for (const auto& pt : points_roof) {
      points_roof_pc.push_back({pt[0], pt[1], pt[2]});
    }
    for (const auto& pt : points_ground) {
      points_ground_pc.push_back({pt[0], pt[1], pt[2]});
    }
    roofer::LinearRing linear_ring;
    convert_to_linear_ring(footprint, linear_ring);
    auto meshes =
        reconstruct(points_roof_pc, points_ground_pc, linear_ring, cfg);
    return convert_meshes_to_py_meshes(meshes);
  }

  std::vector<PyMesh> py_reconstruct(
      const PyPointCollection& points_roof, const PyLinearRing& footprint,
      ReconstructOptions cfg = ReconstructOptions()) {
    PointCollection points_roof_pc;
    for (const auto& pt : points_roof) {
      points_roof_pc.push_back({pt[0], pt[1], pt[2]});
    }
    roofer::LinearRing linear_ring;
    convert_to_linear_ring(footprint, linear_ring);
    auto meshes = reconstruct(points_roof_pc, linear_ring, cfg);
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

template <typename Config>
void bind_component_config(py::module_& m, const char* name) {
  py::class_<Config> binding(m, name);
  binding.def(py::init<>());
  Config::visit_fields([&](auto field) {
    if (field.visibility == roofer::config::Visibility::public_) {
      binding.def_readwrite(field.name.data(), field.member);
    }
  });
}

void warn_deprecated(const char* name, const char* replacement) {
  auto warnings = py::module_::import("warnings");
  warnings.attr("warn")(
      std::string(name) + " is deprecated; use " + replacement,
      py::module_::import("builtins").attr("DeprecationWarning"), 2);
}

template <typename Accessor>
void bind_deprecated_property(py::class_<roofer::ReconstructOptions>& binding,
                              const char* name, const char* replacement,
                              Accessor accessor) {
  using Value = std::remove_cvref_t<
      std::invoke_result_t<Accessor, roofer::ReconstructOptions&>>;
  binding.def_property(
      name,
      [name, replacement, accessor](roofer::ReconstructOptions& self) -> Value {
        warn_deprecated(name, replacement);
        return accessor(self);
      },
      [name, replacement, accessor](roofer::ReconstructOptions& self,
                                    Value value) {
        warn_deprecated(name, replacement);
        accessor(self) = std::move(value);
      });
}

std::string component_class_name(std::string_view component_name) {
  std::string result;
  bool capitalise = true;
  for (const char character : component_name) {
    if (character == '-') {
      capitalise = true;
    } else {
      result.push_back(capitalise ? static_cast<char>(std::toupper(character))
                                  : character);
      capitalise = false;
    }
  }
  return result + "Config";
}

std::string component_property_name(std::string_view component_name) {
  std::string result(component_name);
  std::replace(result.begin(), result.end(), '-', '_');
  return result;
}

PYBIND11_MODULE(roofer, m) {
  using namespace roofer::reconstruction;
  py::enum_<roofer::enums::TerrainStrategy>(m, "TerrainStrategy")
      .value("BUFFER_TILE", roofer::enums::BUFFER_TILE)
      .value("BUFFER_USER", roofer::enums::BUFFER_USER)
      .value("USER", roofer::enums::USER);
  roofer::reconstruction::ReconstructionConfig component_registry;
  component_registry.visit_components(
      [&](std::string_view name, auto& component) {
        using Config = std::remove_cvref_t<decltype(component)>;
        if (!roofer::config::has_public_fields<Config>()) return;
        const auto class_name = component_class_name(name);
        bind_component_config<Config>(m, class_name.c_str());
      });

  py::class_<roofer::reconstruction::ReconstructionConfig> pipeline_config(
      m, "ReconstructionPipelineConfig");
  pipeline_config.def(py::init<>());
  roofer::reconstruction::ReconstructionConfig::visit_fields([&](auto field) {
    pipeline_config.def_readwrite(field.name.data(), field.member);
  });
  roofer::reconstruction::ReconstructionConfig::visit_component_fields(
      [&](std::string_view name, auto member) {
        using Config =
            std::remove_cvref_t<decltype(component_registry.*member)>;
        if (!roofer::config::has_public_fields<Config>()) return;
        const auto property_name = component_property_name(name);
        pipeline_config.def_readwrite(property_name.c_str(), member);
      });

  py::class_<roofer::ReconstructOptions> reconstruct_config(
      m, "ReconstructionConfig");
  reconstruct_config.def(py::init<>())
      .def_readwrite("reconstruction",
                     &roofer::ReconstructOptions::reconstruction)
      .def_readwrite("lod", &roofer::ReconstructOptions::lod);

  roofer::reconstruction::ReconstructionConfig::visit_component_fields(
      [&](std::string_view name, auto member) {
        using Config =
            std::remove_cvref_t<decltype(component_registry.*member)>;
        if (!roofer::config::has_public_fields<Config>()) return;
        const auto property_name = component_property_name(name);
        reconstruct_config.def_property_readonly(
            property_name.c_str(),
            [member](roofer::ReconstructOptions& self) -> Config& {
              return self.reconstruction.*member;
            },
            py::return_value_policy::reference_internal);
      });

  reconstruct_config
      .def_readwrite("floor_elevation",
                     &roofer::ReconstructOptions::floor_elevation)
      .def_readwrite("override_with_floor_elevation",
                     &roofer::ReconstructOptions::override_with_floor_elevation)
      .def("is_valid", &roofer::ReconstructOptions::is_valid);

  bind_deprecated_property(reconstruct_config, "lod13_step_height",
                           "reconstruction.lod13_step_height",
                           [](roofer::ReconstructOptions& self) -> float& {
                             return self.reconstruction.lod13_step_height;
                           });
  bind_deprecated_property(
      reconstruct_config, "complexity_factor",
      "reconstruction.arrangement_optimiser.complexity_factor",
      [](roofer::ReconstructOptions& self) -> float& {
        return self.reconstruction.arrangement_optimiser.complexity_factor;
      });
  bind_deprecated_property(reconstruct_config, "clip_ground",
                           "reconstruction.clip_terrain",
                           [](roofer::ReconstructOptions& self) -> bool& {
                             return self.reconstruction.clip_terrain;
                           });
  bind_deprecated_property(
      reconstruct_config, "plane_detect_k",
      "reconstruction.plane_detector.plane_neighbour_count",
      [](roofer::ReconstructOptions& self) -> int& {
        return self.reconstruction.plane_detector.plane_neighbour_count;
      });
  bind_deprecated_property(
      reconstruct_config, "plane_detect_min_points",
      "reconstruction.plane_detector.min_plane_points",
      [](roofer::ReconstructOptions& self) -> int& {
        return self.reconstruction.plane_detector.min_plane_points;
      });
  bind_deprecated_property(
      reconstruct_config, "plane_detect_epsilon",
      "reconstruction.plane_detector.plane_epsilon",
      [](roofer::ReconstructOptions& self) -> float& {
        return self.reconstruction.plane_detector.plane_epsilon;
      });
  bind_deprecated_property(
      reconstruct_config, "plane_detect_normal_angle",
      "reconstruction.plane_detector.plane_normal_threshold",
      [](roofer::ReconstructOptions& self) -> float& {
        return self.reconstruction.plane_detector.plane_normal_threshold;
      });
  bind_deprecated_property(
      reconstruct_config, "line_detect_epsilon",
      "reconstruction.line_detector.distance_threshold",
      [](roofer::ReconstructOptions& self) -> float& {
        return self.reconstruction.line_detector.distance_threshold;
      });
  bind_deprecated_property(reconstruct_config, "thres_alpha",
                           "reconstruction.alpha_shaper.alpha",
                           [](roofer::ReconstructOptions& self) -> float& {
                             return self.reconstruction.alpha_shaper.alpha;
                           });
  bind_deprecated_property(
      reconstruct_config, "thres_reg_line_dist",
      "reconstruction.line_regulariser.distance_threshold",
      [](roofer::ReconstructOptions& self) -> float& {
        return self.reconstruction.line_regulariser.distance_threshold;
      });
  bind_deprecated_property(
      reconstruct_config, "thres_reg_line_ext",
      "reconstruction.line_regulariser.extension",
      [](roofer::ReconstructOptions& self) -> float& {
        return self.reconstruction.line_regulariser.extension;
      });

  m.attr("ReconstructOptions") = m.attr("ReconstructionConfig");
  m.attr("NestedReconstructionConfig") = m.attr("ReconstructionPipelineConfig");

  m.def("reconstruct",
        py::overload_cast<const PyPointCollection&, const PyPointCollection&,
                          const PyLinearRing&, roofer::ReconstructOptions>(
            &roofer::py_reconstruct),
        "Reconstruct a single instance of a building from a point cloud with "
        "ground points",
        py::arg("points_roof"), py::arg("points_ground"), py::arg("footprint"),
        py::arg("cfg") = roofer::ReconstructOptions());

  m.def("reconstruct",
        py::overload_cast<const PyPointCollection&, const PyLinearRing&,
                          roofer::ReconstructOptions>(&roofer::py_reconstruct),
        "Reconstruct a single instance of a building from a point cloud "
        "without ground points",
        py::arg("points_roof"), py::arg("footprint"),
        py::arg("cfg") = roofer::ReconstructOptions());

  m.def("triangulate_mesh", &roofer::py_triangulate_mesh, "Triangulate a mesh",
        py::arg("mesh"));
}
