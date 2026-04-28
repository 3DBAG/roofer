# Building from source

## Compilation with Conan

Conan is the recommended way to build roofer from source.

Install Conan 2 and then configure and build the project like this:

```sh
git clone https://github.com/3DBAG/roofer.git
cd roofer
conan profile detect --force
conan install . \
  --output-folder=build \
  --build=missing \
  --settings=build_type=Release \
  --settings=compiler.cppstd=20 \
  --options="&:build_apps=True" \
  --options="&:use_spdlog=True" \
  --options="&:use_val3dity=False" \
  --options="&:build_bindings=False" \
  --options="&:build_testing=False"
# Conan forwards the package options above to the matching RF_* CMake options.
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$PWD/install
cmake --build build
# Optionally, install roofer
cmake --install build
```

## Compilation with Nix

If you prefer Nix, you can use the provided development shell. At this moment Nix only works on Linux and macOS.

Once Nix is installed you can set up the development environment and build roofer like this:

```sh
git clone https://github.com/3DBAG/roofer.git
cd roofer
nix develop
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRF_BUILD_APPS=ON \
  -DRF_USE_LOGGER_SPDLOG=ON \
  -DRF_USE_VAL3DITY=OFF \
  -DRF_BUILD_BINDINGS=OFF \
  -DRF_BUILD_TESTING=OFF \
  -DRF_USE_CPM=OFF
cmake --build build
# Optionally, install roofer
cmake --install build
```

If you just want the packaged build outputs, `nix build` also works:

```sh
nix build .#default
nix build .#rooferpy
```

## Documentation

To build the documentation locally, first build the documentation helper and Python bindings.

### With Conan

```sh
conan profile detect --force
conan install . \
  --output-folder=build \
  --build=missing \
  --settings=build_type=Release \
  --settings=compiler.cppstd=20 \
  --options="&:build_apps=False" \
  --options="&:use_spdlog=False" \
  --options="&:use_val3dity=False" \
  --options="&:build_bindings=True" \
  --options="&:build_testing=False"
# Conan forwards the package options above to the matching RF_* CMake options.
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$PWD/install \
  -DRF_BUILD_DOC_HELPER=ON
cmake --build build --target rooferpy doc-helper
cmake --install build
cd docs
make html
```

### With Nix

```sh
nix develop
cmake -S . -B build \
  -G Ninja \
  -DRF_BUILD_APPS=OFF \
  -DRF_USE_LOGGER_SPDLOG=OFF \
  -DRF_USE_VAL3DITY=OFF \
  -DRF_BUILD_BINDINGS=ON \
  -DRF_BUILD_TESTING=OFF \
  -DRF_BUILD_DOC_HELPER=ON \
  -DRF_USE_CPM=OFF
cmake --build build --target rooferpy doc-helper
cmake --install build
cd docs
make html
```
