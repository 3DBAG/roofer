# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

This releases contains several bugfixes, stability improvements, and new functionalities. Some highlights include:

1. significantly improved geometric validity of the generated building models; in the order of 10x less errors with val3dity. This was achieved by detecting and fixing locally 2D arrangement configurations that led to e.g. non-manifold edges after extrusion and self intersecting faces. Likewise it is now detected if a slanted roofplane would be extruded to under the floor elevation, the below the floor part is now cut off from the footprint.
2. User access to all reconstruction parameters.
3. Reorganised config file. Parameters are now divided into Input, Crop, Reconstruction, and Output sections. Old parameters locations will give a deprecation warning, and be removed in v2.0.
4. A new `unit-scale` parameter that scales parameter values that are defined in metres. For example if you data is in feet, this can be used to get good results with the default parameter values. One user also reported that a scale of 0.55 can help with getting better results on low density point clouds.


### Added
- Repair of non-manifold and self-intersecting roof junctions during arrangement snapping. This prevents certain geometric errors like non-watertight meshes, and highly non-planar faces that could previously happen in complex roof arrangements. See #168.
- Experimental optional per-tile triangulated terrain output as CityJSON `TINRelief` features, see the `--terrain` option. Its output and configuration may change, and the feature may be removed in a future release.
- A new `max-nodata-fraction` configuration option to control the maximum fraction of nodata pixels in a building polygon that is allowed before a building pointcloud is considered insufficient.
- A descriptor-backed `roofer::reconstruction::ReconstructionConfig` that contains the configuration for every reconstruction stage. Field declarations are now the single source for defaults, descriptions, validation, nested TOML parsing, generated documentation, and Python bindings.
- A nested C++ `ReconstructOptions` API and corresponding `reconstruct` overload. The nested reconstruction configuration is also available from Python, including each component configuration that exposes public parameters.
- Arm64 docker image

### Fixed
- More robust handling of OGC WKT payloads in LAS/LAZ files.
- Prevent roof-ground bow-ties by clipping roof faces against the terrain before extrusion.
- Correct squared-distance tolerance comparisons during extrusion.
- Reject TOML integer values outside the supported `int` range instead of silently narrowing them.
- Reject malformed or non-finite numeric CLI and TOML values instead of accepting numeric prefixes or propagating `NaN`/infinity. CLI `--no-` options are now limited to boolean parameters, general CLI validators are applied consistently, and TOML syntax errors retain their source location and parser explanation.

### Changed
- Application configuration is organised into `[input]`, `[crop]`, `[reconstruction]`, and `[output]` tables, with output attribute names under `[output.attributes]` and pointclouds under `[[input.pointclouds]]`. Deprecated root keys remain supported during 1.x, while canonical section values take precedence. Generated example configuration now uses the canonical layout.
- The pointcloud-insufficient test is now an absolute per-building density floor (`min-building-density`, points/m²) instead of a tile-relative `mean - 2·std` outlier test. The old test depended the entire tile that was being processed, so the same building could be flagged differently between runs with different tiling — potentially skipping point-rich buildings and leaving them without geometry. The decision is now deterministic and depends only on the building's own data.
- Region growing no longer uses a wall-clock time limit, which could make a building reconstruct with success on one run and fall back to LoD 1.1 on the next given identical input due to resource starvation with multithreading. The deterministic plane-count limit (`lod11-fallback-planes`) is now the sole reconstruction-complexity cutoff. The `lod11-fallback-time` behavior is removed; its CLI flag and root configuration key are accepted but ignored with a deprecation warning during 1.x compatibility.
- Reconstruction configuration now uses nested TOML tables such as `[reconstruction.plane-detector]` and `[reconstruction.arrangement-optimiser]`. Core reconstruction stages receive their component configuration aggregates; pipeline-derived settings such as requested LoD and terrain availability are overlaid on local copies.
- Reconstruction parameter descriptions now state physical and angular units where applicable; counts remain unit-free.
- All canonical reconstruction angle parameters now use degrees. Plane-detection normal, horizontal, and wall thresholds are expressed directly as angles instead of dot products, and the line-regularisation  angle is no longer in radians. The legacy `plane-detect-normal-angle` alias and compatibility API properties retain their 1.x dot-product semantics.
- Reconstruction snapping and duplicate-vertex tolerances now use distances in metres instead of base-10 exponents. Existing defaults retain their previous effective distances.
- Added global `unit-scale` (metres per input coordinate unit) to convert metre-based reconstruction, crop, and output parameters for input data that uses another unit.
- Set the default cityjson scale in the transform object to 0.0001. This helps with improving geometric validity. The previous 0.001 setting regurlarly caused validity issues due to the relatively large quatisation error.
- Descriptor-generated TOML documentation and Python bindings omit reconstruction components that have no public parameters.
- `complexity-factor` now directly controls the optimiser energy balance: the data term is weighted by `complexity-factor` and the smoothness term by `1 - complexity-factor`. The independent `data_multiplier` and `smoothness_multiplier` component options were removed.
- The flattened C++ `roofer::ReconstructionConfig` remains as a 1.x compatibility adapter, while its `reconstruct` overloads are deprecated. Its defaults and validation delegate to the nested configuration.
- Low-level C++ component configuration member names were normalised to `snake_case`, and obsolete members were removed. This is a source-breaking change for code that uses the component configuration structs directly; the flattened `roofer::ReconstructionConfig` adapter does not cover those direct uses.
- Legacy root-level reconstruction TOML keys remain accepted during 1.x, emit a destination-specific deprecation warning, and are applied before nested TOML values. CLI arguments retain the highest precedence. The `--lod12`, `--lod13`, `--lod22`, `--clip-terrain`, and `--complexity-factor` flags remain supported without deprecation warnings. Other supported legacy reconstruction CLI flags remain accepted with deprecation warnings; several older names remain root-TOML-only aliases and are not accepted as CLI flags.
- Use mesh centroid for volume calculation.

### Reconstruction parameter mapping

Legacy root keys, and matching legacy CLI flags where available, map as follows:

| Legacy parameter | Nested TOML parameter | Notes |
|---|---|---|
| `lod12` | `[reconstruction] lod12` | CLI flag remains supported. |
| `lod13` | `[reconstruction] lod13` | CLI flag remains supported. |
| `lod22` | `[reconstruction] lod22` | CLI flag remains supported. |
| `clip-terrain` | `[reconstruction] clip-terrain` | CLI flag remains supported. |
| `lod13-step-height` | `[reconstruction] lod13-step-height` | Relocated; legacy CLI flag is deprecated. |
| `h-terrain-strategy` | `[reconstruction] h-terrain-strategy` | Relocated; legacy CLI flag is deprecated. |
| `complexity-factor` | `[reconstruction.arrangement-optimiser] complexity-factor` | CLI flag remains supported. |
| `plane-detect-k` | `[reconstruction.plane-detector] plane-neighbour-count` | Renamed; legacy CLI flag is deprecated. |
| `plane-detect-min-points` | `[reconstruction.plane-detector] min-plane-points` | Renamed; legacy CLI flag is deprecated. |
| `plane-detect-epsilon` | `[reconstruction.plane-detector] plane-epsilon` | Renamed; legacy CLI flag is deprecated. |
| `plane-detect-normal-angle` | `[reconstruction.plane-detector] normal-angle-threshold` | Renamed; root TOML alias only. Legacy values remain unitless dot products and are converted to degrees. |
| `line-detect-epsilon` | `[reconstruction.line-detector] distance-threshold` | Renamed; root TOML alias only. |
| `thres-alpha` | `[reconstruction.alpha-shaper] alpha` | Renamed; root TOML alias only. |
| `thres-reg-line-dist` | `[reconstruction.line-regulariser] distance-threshold` | Renamed; root TOML alias only. |
| `thres-reg-line-ext` | `[reconstruction.line-regulariser] extension` | Renamed; root TOML alias only. |
| `lod11-fallback-planes` | `[reconstruction.plane-detector] max-plane-count` | Renamed; deprecated legacy CLI flag and deterministic complexity cutoff. |
| `lod11-fallback-time` | `[reconstruction.plane-detector] max-plane-count` | Deprecated CLI flag and root key are accepted but ignored; `max-plane-count` is the deterministic replacement. |

## [1.0.0] - 2026-04-20

## Added
- CLI: new short help message via `--help`/`-h`, previous long help message is now printed with `--help-all`/`-H`
- Improved documentation for `--filter` option

## Changed
- CLI: `--jobs`/`-j` now assigns roughly `jobs - 1` threads to reconstruction instead of reserving five internal threads first.
- Invalid footprint features are skipped, instead of failing the whole tile
- Terrain height fallback now uses a local terrain grid before falling back to the tile minimum.
- Improved default log messages
- Improved install script
- Fixed compile warnings for ptinpoly.c

## [1.0.0-beta.6] - 2026-04-17

### Fixed
- Prevent building terrain elevation getting assigned a garbage value in case of multiple input pointclouds when one of them has no points
- Floating point exception when the first and last point are the same in LinearRing holes
- potential segfault with roofer --help

### Changed
- Revamp recommended build system to use conan instead of vcpkg. Easier to set up and maintain, smaller build artifacts, faster builds.
- Add support for Nix builds

## [1.0.0-beta.5] - 2025-08-27

### Fixed
- Fix for rare cases where RoofSurfaces did not get height attributes
- Fix illegal values for building terrain height in case of no terrain points near footprint

## [1.0.0-beta.4] - 2025-08-25

### Fixed
- Do not append an `_` to some attribute names (eg. t_run, t_pc_source)

## [1.0.0-beta.3] - 2025-08-19

### Fixed
- The `--no-clear-insuffient` flag now works as expected.

### Changed
- In case of a failure during reconstruction, the mesh geomtries of all LoDs are cleared.
- Extrusion_mode is now set to 'fail' if reconstruction fails
- Only define CGAL_ALWAYS_ROUND_TO_NEAREST on arm64. This fixes some issues with infinite loops on other architectures.

## [1.0.0-beta.2] - 2025-08-04

### Added
- install script for curl pipe install
- automatic versioning
- CLI flag to disable input polygon simplification:: `--no-simplify`
- allow to omit output attributes by renaming them to an empty string

### Fixed
- fix handling of negative flags like --no-lod22
- fix handling of polygon inputs with duplicate vertices
- more robust calculation of nodata circle
- fix bug causing skipping reconstruction of some buildings with flat roofs
- fix issue with SegmentRasteriser that sometimes led to very high memory usage
- fix bug that caused incorrect height attribute calculation foor roofparts
- fix incorrect use of reserve/resize

### Changed
- WKT logging from geos module now prints true coordinates instead of translated ones
- Turn off fill_nodata_ in SegmentRasteriser by default.
- gridthinPointcloud: use fixed seed to make output deterministic over multiple runs

## [1.0.0-beta.1] - 2025-05-28

### Added
- add Nix flake for easy setting up of reproducable build environment
- automatic documentation generation for CLI options and output attributes
- new `--attributes` flag that lists output attributes
- better CLI --help formatting
- new `--attribute-rename` option to rename output attributes
- boolean option can no be disabled on the CLI by preprending `no-`
- output attribute with main ridgeline elevation

### Fixed
- Make global h attribute calculation less sensitive to outliers

### Changed
- What LoD's are generated is now specified via boolean options `lod12`, `lod13`, and `lod22`
- Document all options
- Document all output attributes
- switch from pip to uv for managing python dependencies
- Tiling is disabled by default
- Output files are now written to `<output-dir>/<minx>_<miny>.city.jsonl`
- Some boolean options have changed in default value
- Switched documentation website from RST to markdown
