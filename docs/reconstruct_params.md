# Reconstruction algorithm

## Assumptions
The following assumptions are made in the reconstruction algorithm:

* Roof shapes are approximated with planar faces extracted from a point cloud -- in other words, they are **piecewise planar**,
* Building models are **2.5D** with vertical walls. This simplifies the problem and results in a fast algorithm and makes sense for Airborne lidar scans; however, this also means that features like roof overhangs and balconies cannot be modelled.
* Point clouds and footprints are properly aligned. Not only must the input data be in the same coordinate reference system (CRS), but also the positional error (horizontal shift) is assumed to be minimal.
* Point clouds should be free of outliers and classified with at least buildings and terrain classes. In case the building points contain vegetation and other off-ground artefacts it might interfere with the reconstruction. For more information on the requirements of the input data, see {doc}`data_requirements`.

## Algorithm overview
```{image} _static/img/algo-steps.png
:alt: The roofer reconstruction algorithm
:width: 600px
:align: center
```

The roofer building reconstruction algorithm consists of the following steps:

1. **Plane detection**: Roofplanes are detected in the point cloud using a region growing algorithm. Planes are used to approximate the roof shape.
2. **Line detection**: Rooflines are detected by intersecting the roofplanes and by fitting lines to the edges of the plane segments. The edges of the planes are detected using {math}`\alpha`-shapes.
3. **Line clustering and regularisation**: The detected lines are clustered by their orientation and distance to eachother. For each cluster one represenative line is selected.
4. **Initial roof-partition**: The regularised rooflines are used to partition the roofprint polygon into faces. This results in a 2D arrangement that we call the roof-partition.
5. **Graph-cut optimisation**: The roof-partition is optimised using a graph-cut algorithm. The algorithm minimises the energy function that balances the data term {math}`D_p\left(f_p\right)` and the smoothness term {math}`S_{p, q}\left(f_p, f_q\right)`:
      ```{math}
         E(f) &= \lambda \sum_{p \in P} D_p\left (f_p\right) \\
            &+ \left( 1 - \lambda \right) \sum_{\{p, q\} \in N} S_{p, q}\left(f_p, f_q\right)
      ```
      During this optimisation step each face {math}`p` is assigned one of the detected roofplanes {math}`f_p`. When to adjacent faces get assigned the same roofplane, the shared edge is removed. The result is the final roof-partition.
      {math}`\lambda` is the complexity factor.
6. **Extrusion**: The final roof-partition is extruded to a 3D building model with the help of the assigned plane labels.

The roofer building reconstruction algorithm is largely data-driven, so the quality of the result depends on the quality of the input data.

## Configuration

Reconstruction settings live in the `[reconstruction]` TOML table. Settings
for individual stages use nested tables such as
`[reconstruction.plane-detector]` and
`[reconstruction.arrangement-optimiser]`. The complete example configuration
is generated from the C++ field descriptors, so it always reflects the runtime
defaults and validation rules.

The former root-level reconstruction keys remain compatibility aliases in the
1.x series. They emit deprecation warnings and are scheduled for removal in
2.0. The `--lod12`, `--lod13`, `--lod22`, `--clip-terrain`, and
`--complexity-factor` CLI flags remain supported and are not deprecated. Other
legacy reconstruction CLI flags emit deprecation warnings. Nested TOML values
take precedence over root-level aliases; command-line arguments take
precedence over both.

The removed `lod11-fallback-time` root key and CLI flag are accepted but
ignored during the 1.x compatibility period and emit a deprecation warning.
Use `max-plane-count` under `[reconstruction.plane-detector]` as the
deterministic reconstruction-complexity cutoff.

Low-level C++ configuration member names were normalised to `snake_case` in
1.x. The flattened reconstruction API remains available as a deprecated
adapter, but code using component structs directly must use the cleaned names;
those direct component member renames cannot be represented by C++ aliases.

<!-- ### Parameters
The following parameters can be tuned to optimise the performance for a given point cloud.

```{doxygenstruct} roofer::reconstruction::ReconstructionConfig
:project: Roofer
:members:
:members-only:
``` -->
