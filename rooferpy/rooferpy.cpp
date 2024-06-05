#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <roofer/roofer.h>

namespace py = pybind11;

namespace roofer {

void convert_to_linear_ring(const std::vector<std::vector<std::array<float, 3>>>& footprint,
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

std::vector<std::vector<std::vector<std::array<float, 3>>>>
convert_meshes_to_linear_rings(const std::vector<Mesh>& meshes) {
    std::vector<std::vector<std::vector<std::array<float, 3>>>> meshes_linear_rings;
    for (const auto& mesh : meshes) {
        std::vector<std::vector<std::array<float, 3>>> one_mesh_linear_rings;
        for (const auto& roofer_linear_ring : mesh.get_polygons()) {
            std::vector<std::array<float, 3>> linear_ring;
            for (const auto& pt : roofer_linear_ring) {
                linear_ring.push_back({pt[0], pt[1], pt[2]});
            }
            one_mesh_linear_rings.push_back(linear_ring);
        }
        meshes_linear_rings.push_back(one_mesh_linear_rings);
    }
    return meshes_linear_rings;
}


std::vector<std::vector<std::vector<std::array<float, 3>>>>
py_reconstruct_single_instance(const std::vector<std::array<float, 3>>& points_roof,
                               const std::vector<std::array<float, 3>>& points_ground,
                               const std::vector<std::vector<std::array<float, 3>>>& footprint,
                               ReconstructionConfig cfg = ReconstructionConfig()) {
    PointCollection points_roof_pc, points_ground_pc;
    for (const auto& pt : points_roof) {
        points_roof_pc.push_back({pt[0], pt[1], pt[2]});
    }
    for (const auto& pt : points_ground) {
        points_ground_pc.push_back({pt[0], pt[1], pt[2]});
    }
    roofer::LinearRing linear_ring;
    convert_to_linear_ring(footprint, linear_ring);
    auto meshes = reconstruct_single_instance(points_roof_pc, points_ground_pc, linear_ring, cfg);
    return convert_meshes_to_linear_rings(meshes);
}

std::vector<std::vector<std::vector<std::array<float, 3>>>>
py_reconstruct_single_instance(const std::vector<std::array<float, 3>>& points_roof,
                               const std::vector<std::vector<std::array<float, 3>>>& footprint,
                               ReconstructionConfig cfg = ReconstructionConfig()) {
    PointCollection points_roof_pc;
    for (const auto& pt : points_roof) {
        points_roof_pc.push_back({pt[0], pt[1], pt[2]});
    }
    roofer::LinearRing linear_ring;
    convert_to_linear_ring(footprint, linear_ring);
    auto meshes = reconstruct_single_instance(points_roof_pc, linear_ring, cfg);
    return convert_meshes_to_linear_rings(meshes);
}

} // namespace roofer

PYBIND11_MODULE(rooferpy, m) {
    py::class_<roofer::ReconstructionConfig>(m, "ReconstructionConfig")
            .def(py::init<>())
            .def_readwrite("complexity_factor", &roofer::ReconstructionConfig::lambda)
            .def_readwrite("clip_ground", &roofer::ReconstructionConfig::clip_ground)
            .def_readwrite("lod", &roofer::ReconstructionConfig::lod)
            .def_readwrite("lod13_step_height", &roofer::ReconstructionConfig::lod13_step_height)
            .def_readwrite("floor_elevation", &roofer::ReconstructionConfig::floor_elevation)
            .def_readwrite("override_with_floor_elevation", &roofer::ReconstructionConfig::override_with_floor_elevation)
            .def_readwrite("verbose", &roofer::ReconstructionConfig::verbose)
            .def("is_valid", &roofer::ReconstructionConfig::is_valid);

    m.def("reconstruct_single_instance",
          py::overload_cast<const std::vector<std::array<float, 3>>&,
                  const std::vector<std::array<float, 3>>&,
                  const std::vector<std::vector<std::array<float, 3>>>&,
                  roofer::ReconstructionConfig>(&roofer::py_reconstruct_single_instance),
          "Reconstruct a single instance of a building from a point cloud with ground points",
          py::arg("points_roof"), py::arg("points_ground"), py::arg("footprint"), py::arg("cfg") = roofer::ReconstructionConfig());

    m.def("reconstruct_single_instance",
          py::overload_cast<const std::vector<std::array<float, 3>>&,
                  const std::vector<std::vector<std::array<float, 3>>>&,
                  roofer::ReconstructionConfig>(&roofer::py_reconstruct_single_instance),
          "Reconstruct a single instance of a building from a point cloud without ground points",
          py::arg("points_roof"), py::arg("footprint"), py::arg("cfg") = roofer::ReconstructionConfig());
}