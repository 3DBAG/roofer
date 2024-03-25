# roofer
Automatic 3D building reconstruction

## Overview src/ folder (possibly subject to change)
`src/common.hpp` and `src/Raster.hpp` contains some basic types to store pointclouds, polygons, rasters etc that are used throughout the library. Only depends on the C++17 standard library.

`src/detection` and `src/partitioning` contain everything needed to perform building reconstruction (see `apps/reconstruct.cpp` for how to use). These files only depend on CGAL, there are not other external dependencies.

`src/io` contains functions to read and write OGR/GDAL datasources and LAS files. Has a bunch of external dependencies like GDAL, GEOS, PROJ, LASlib

`src/misc`, `src/quality` various function used by the `crop` application.

## Roofer apps

### crop
Takes a bunch of input las files and footprints and outputs a folder hierarchy with reconstruction inputs for each separate building

### reconstruct
Takes a point cloud and footprint for a single building and performs building reconstruction. 

Currently it is using rerun.io to visualise the result. You need to install the rerun viewer if you want to see the output.

![reconstruct output visualised with Rerun](rerun.png)

## Installation

clone this repository. Then

```
cd roofer-dev
git submodule update --init --recursive
mkdir build
cd build
cmake ..
cmake --build . --parallel 10
```

This assumes you have CGAL, GDAL, PROJ, GEOS, LASlib  preinstalled on your system

## Run with test-data
Assuming you build roofer successfully. Unzip the contents of [wippolder.zip](https://data.3dgi.xyz/geoflow-test-data/wippolder.zip) into `test-data`, then

```
cd test-data
../build/apps/crop -c crop_config.toml
../build/apps/reconstruct --verbose
```