# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!-- ## [1.0.0] - 2025-05-27 -->
## [Unreleased]

### Fixed
- fix handling of negative flags like --no-lod22

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
