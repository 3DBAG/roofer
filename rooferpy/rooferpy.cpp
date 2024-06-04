#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <roofer/roofer.h>

namespace py = pybind11;

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
          py::overload_cast<const roofer::PointCollection&, const roofer::PointCollection&, roofer::LinearRing&, roofer::ReconstructionConfig>(&roofer::reconstruct_single_instance<roofer::LinearRing>),
          "Reconstruct a single instance of a building from a point cloud with ground points",
          py::arg("points_roof"), py::arg("points_ground"), py::arg("footprint"), py::arg("cfg") = roofer::ReconstructionConfig());

    m.def("reconstruct_single_instance", 
          py::overload_cast<const roofer::PointCollection&, roofer::LinearRing&, roofer::ReconstructionConfig>(&roofer::reconstruct_single_instance<roofer::LinearRing>),
          "Reconstruct a single instance of a building from a point cloud without ground points",
          py::arg("points_roof"), py::arg("footprint"), py::arg("cfg") = roofer::ReconstructionConfig());
}
