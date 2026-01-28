## FlatCityBuf (FCB) output – setup and usage

This branch adds **FlatCityBuf (FCB)** output to `roofer`, implemented in **Rust** using the `fcb_core` crate and the `cxx` C++/Rust bridge. The existing C++ reconstruction and serialization pipeline remains intact; FCB is an **optional output format** selected via the CLI.

This document explains what reviewers / users need to have in place for the code in this branch to work.

---

### 1. Prerequisites

- **C++ toolchain**: Same as upstream `roofer` (CMake, a recent Clang/GCC, etc.).
- **vcpkg**: As per the existing project instructions (already wired through CMake).
- **Rust toolchain**:
  - A recent **stable** Rust via `rustup` (e.g. `rustup default stable`).
  - `~/.cargo/bin` should be on your `PATH`.

If you use **Nix**, the flake in this repo will provide a matching environment, including Rust.

---

### 2. Recommended: build inside the Nix dev shell

If you have Nix installed:

```bash
cd /path/to/roofer

# Enter the dev environment (provides C++, Rust, vcpkg, etc.)
nix develop

# Configure (if you don’t already have a build directory)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 1) Build the Rust FCB FFI library and cxx bridge
cmake --build build --target roofer_fcb_ffi_build -j"$(nproc)"

# 2) Build roofer itself
cmake --build build --target roofer -j"$(nproc)"
```

The `roofer_fcb_ffi_build` target:

- Builds the Rust crate `roofer-fcb-ffi` in release mode.
- Runs `cxx-build` to generate the C++ bridge (`lib.rs.h` / `.cc`) and the `libcxxbridge1.a` runtime.
- Copies the generated headers and source into the CMake include path so the C++ `FcbWriter` can link against them.

---

### 3. Building without Nix (pure CMake + Rust)

If you prefer not to use Nix:

- Ensure `rustup` is installed and a **stable** toolchain is available:

```bash
rustup default stable
```

- Ensure `~/.cargo/bin` is on your `PATH` so `cargo` and `rustc` are visible to CMake.

Then:

```bash
cd /path/to/roofer

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build --target roofer_fcb_ffi_build -j"$(nproc)"
cmake --build build --target roofer -j"$(nproc)"
```

The top-level `CMakeLists.txt` has been wired so that:

- The Rust FFI is built via `roofer_fcb_ffi_build`.
- The generated bridge code and `libcxxbridge1.a` are linked into the `roofer` executable.

No extra manual steps are required beyond having a working Rust toolchain.

---

### 4. Using FCB output at runtime

- **CLI flag**: FCB output is enabled via the existing `--output-format` setting:

```bash
./build/apps/roofer-app/roofer --output-format fcb <pointcloud> <footprints> <output-dir>
```

- **File naming**:
  - In **non-split** mode, the `.fcb` file name is derived from the first input point cloud’s base name, plus the tile coordinates, e.g.:
    - Input: `CG11329_2018_PointCloud_CGVD2013.laz`
    - Output: `CG11329_2018_PointCloud_CGVD2013_449555_4944094.fcb`
  - In **split** mode (one file per building), each building gets its own `.fcb` file based on the JSONL naming scheme.

- **No spurious JSONL files**:
  - The existing `CityJsonWriterInterface` still requires an `std::ostream`, but in FCB mode we now pass a **dummy `std::ostringstream`** so:
    - No `.city.jsonl` file is created.
    - The C++ pipeline is left essentially unchanged.

---

### 5. FCB writing semantics (high level)

- **Rust side**:
  - The Rust crate `roofer-fcb-ffi`:
    - Reads a minimal CityJSON header (transform, metadata) from a temp JSON file.
    - Constructs an `fcb_core::FcbWriter` with:
      - `HeaderWriterOptions { feature_count = N, write_index = true, index_node_size = 16, .. }`
      - This ensures the header has a **non-zero `features_count`** and a spatial index, as required by `fcb_core::FcbReader` and tools like Tyler.
    - Receives per-building CityJSONFeature JSON strings from C++ and calls `FcbWriter::add_feature`.
    - On `close()`, writes the full FCB file (magic bytes, header, R-tree, features) to disk via a buffered writer, flushing and syncing the file.

- **C++ side**:
  - A new `roofer::io::FcbWriter` implements `CityJsonWriterInterface`:
    - It mirrors the existing `CityJsonWriter` but:
      - Builds CityJSONFeature JSON in memory.
      - Forwards it to Rust via the `cxx` bridge.
    - Before `write_metadata` in FCB mode, the serializer calls:
      - `FcbWriter::set_expected_feature_count(building_tile.buildings.size())`
      - This is passed through to `HeaderWriterOptions::feature_count`.
    - After all features in a tile (or per-building in split mode), the C++ `FcbWriter` calls the Rust `close()` function, which in turn calls `FcbWriter::write`.

---

### 6. Verifying the FCB output

The Rust crate also contains a small helper binary to introspect FCB headers:

```bash
cd roofer-fcb-ffi
cargo run --release --bin inspect_fcb -- /path/to/output.fcb
```

This prints:

- `features_count`
- `index_node_size`
- Whether an attribute index is present

---

### 7. Summary for reviewers

- **Impact on existing code**:
  - C++ reconstruction and JSONL paths are untouched except for a small conditional in the serializer.
  - FCB support is encapsulated in:
    - A new C++ `FcbWriter` implementing `CityJsonWriterInterface`.
    - A Rust crate `roofer-fcb-ffi` that wraps `fcb_core` via `cxx`.
- **How to build**:
  - Enter Nix dev shell (recommended) or ensure a stable Rust toolchain is installed.
  - Build `roofer_fcb_ffi_build`, then build `roofer`.
- **How to use**:
  - Add `--output-format fcb` to the existing CLI.
  - Open the resulting `.fcb` in Tyler or via `fcb_core::FcbReader`.
- **Correctness**:
  - Header `features_count` and index fields are set correctly.
  - FCB files are recognized as indexed by `fcb_core`-based tools.
  - JSONL parity (feature count and metadata) can be validated by comparing an FCB file against its corresponding CityJSONSeq/JSONL output using `fcb_core::FcbReader` and existing Roofer outputs.

