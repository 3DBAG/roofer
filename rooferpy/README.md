# Python bindings for roofer C++ API
We use pybind11 for python bindings. To use the bindings, build with either Conan or Nix.

With Conan:

```
cd roofer-dev
conan profile detect --force
conan install . \
  --output-folder=build_python \
  --build=missing \
  --settings=build_type=Release \
  --settings=compiler.cppstd=20 \
  --options="&:build_apps=False" \
  --options="&:use_spdlog=False" \
  --options="&:use_val3dity=False" \
  --options="&:build_bindings=True" \
  --options="&:build_testing=False"
cmake -S . -B build_python \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=build_python/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DRF_BUILD_APPS=OFF \
  -DRF_USE_LOGGER_SPDLOG=OFF \
  -DRF_USE_VAL3DITY=OFF \
  -DRF_BUILD_BINDINGS=ON \
  -DRF_BUILD_TESTING=OFF \
  -DRF_USE_CPM=OFF
cmake --build build_python --target rooferpy
```

With Nix:

```
cd roofer-dev
nix develop
cmake -S . -B build_python \
  -G Ninja \
  -DRF_BUILD_APPS=OFF \
  -DRF_USE_LOGGER_SPDLOG=OFF \
  -DRF_USE_VAL3DITY=OFF \
  -DRF_BUILD_BINDINGS=ON \
  -DRF_BUILD_TESTING=OFF \
  -DRF_USE_CPM=OFF
cmake --build build_python --target rooferpy
```

The rooferpy library will be located in `build_python/rooferpy/roofer.cpython-<version-and-system>.so`. Import the .so file (e.g. place it in the same folder as .py script) to use roofer python API.
