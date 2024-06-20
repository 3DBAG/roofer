# Python bindings for roofer C++ API
We use pybind11 for python bindings. To use the bindings, make sure to [install pybind11](https://pybind11.readthedocs.io/en/latest/installing.html#include-with-pypi) and compile the source using the `vcpkg-with-bindings` preset, i.e.

```
cd roofer-dev
mkdir build_python
cmake --preset vcpkg-with-bindings -S . -B build_python
cmake --build build_python
```

The rooferpy library will be located in `build/rooferpy/rooferpy.cpyton-<version-and-system>.so`. Import the .so file (e.g. place it in the same folder as .py script) to use roofer python API.


todo: information on data structures
