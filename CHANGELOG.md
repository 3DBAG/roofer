# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- Prevent building terrain elevation getting assigned a garbage value in case of multiple input pointclouds when one of them has no points

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
