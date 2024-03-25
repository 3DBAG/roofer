# roofer: Automatic 3D building reconstruction

# Problem statement

# Background

# Requirements

# Overview of technical challenges

# Design

## Current state (`bd7dee52`)

### High level overview

Roofer reconstructs one building at a time.
Efficient operation requires that the point cloud is cropped to the extent of the building polygon.

There are many parameters that configure the input, output and operation of roofer.
Configuration is passed to roofer in TOML files exclusively. 
The TOML config path is passed with the `-c, --config` command line arguments.

There are two command line applications:

- *crop* (`apps/crop.cpp`): Preprocess the point cloud by cropping with the building polygons.
- *reconstruct* (`apps/reconstruct.cpp`): Reconstruct one building.

#### Dependencies

- [tomlplusplus](https://github.com/marzer/tomlplusplus): TOML parser.
  - Used in the CLI apps.
- [argh](https://github.com/adishavit/argh): Argument handler.
  - Used in the CLI apps.
- [ptinpoly](https://erich.realtimerendering.com/ptinpoly/): Efficient point-in-polygon test.
  - --> PointcloudRasteriser
    - ::isMutated --> *crop*
    - ::computeNoDataFraction --> *crop*
    - ::computePointDensity --> *crop*
    - ::gridthinPointcloud --> *crop*
    - ::isMutated --> select_pointcloud::selectPointCloud --> *crop*
  - StreamCropper
    - ::PointsInPolygonsCollector --> ::PointCloudCropper --> *crop*
- [fmt](https://fmt.dev/latest/index.html): CLI message formatting, path formatting.
  - Only used in the CLI apps.
- [spdlog](https://github.com/gabime/spdlog): Logging. Uses *fmt* internally and depends on it.
  - Used throughout the code.
- GEOS
- laslib
- CGAL::Core, v5.4
- GDAL
- PROJ v9.0.0

#### crop

Parse configuration from the TOML file.
Input data attribtues that control the operation:

- low_lod_attribute
- year_of_construction_attribute

Read vector footprints with VectorReaderOGR::readPolygons (GDAL), including inner rings, attributes and CRS definition. 

  - Attributes: The `output_fid_` variable controls whether to get the OGR_FID from the data, and store it in the `OGR_FID` attribute.
  - CRS: Get the CRS definition as WKT and init the internal projection of roofer from it, using the projHelperInterface.
  - Geometry: Supports only Polygon and MultiPolygon. Polygons are extracted from a MultiPolygon and stored as a LinearRing. The exterior rings are ordered CCW, interiors are CW on reading. Coordinates are transformed to the internal CRS.

Simplify the polygons.

Buffer the polygons.

Crop each input point cloud with the buffered footprints.
Uses PointCloudCropper process each point cloud:
  - read
  - reproject
  - instersect with the polygons
  - crop using lastools AOI indexed intersection test
  - get the point cloud acquisition year
  - compute point cloud quality with PointsInPolygonsCollector::do_post_process

Compute point cloud quality with PointcloudRasteriser methods.
Assign the raster quality stats to the footprint attributes.

Select the suitable point cloud with PointcloudRasteriser::isMutated.

Apply thinning to the point cloud. The bool `low_lod_attribute` applies extra thinning. Used for the `kas_warenhuis` in the 3DBAG.

Compute the nodata radius of the pointcloud. If the  `low_lod_attribute` is set, do not compute.

Create the configuration for the reconstruction.
Write the cropped point cloud. Also write the quality raster if `--rasters` is set.
Write the footprint.
Write the footprints ?again as the index file?
Write CityJSON Sequence paths to .txt.
Write CityJSON metadata.json.

#### reconstruct

init projection

Read the point cloud with createPointCloudReaderLASlib into a PointCollection and a vector of point classes.
Why use a separate vec for the classes instead of the AttributeVecMap?

points --> plane detection (createPlaneDetector) --> planes

planes --> alpha shapes (createAlphaShaper) --> rings, roofplane_ids

rings, roofplane_ids, planes --> line detection (createLineDetector)

planes --> plane intersection (createPlaneIntersector)



### Structures

common::LinearRing : common::Geometry - Represents 3D polygons with inner rings.
  - has inner rings
  - coordinates are `std::array<float, 3>`
  - geometry is stored in the `interior_rings_` member
  - `get_data_ptr` returns a pointer to the outer ring
  - the complete geometry (incl. inner rings) are accessed with `interior_rings`

common::PointCollection : common::GeometryCollection : common::Geometry - Stores the point cloud as a vector of `std::array<float, 3>`.
  - each point can have attributes, backed by an AttributeVecMap
  - `get_data_ptr` returns a pointer to the first point
  - can compute its bounding box

common::AttributeVecMap - Stores the attributes of the input polygons as a vector of values per attribute.
  - an `std::unordered_map` of `std::string` + vector, each member stores the complete set of values for all polygons for the attribute
  - possible attribute value types: bool, int, float, string, 3d array of float (3d point), Date, Time, DateTime

geometry::Vector2DOpsInterface - Operations on 2D vector geometry with GEOS.
  - simplification
  - buffer

crop::InputPointcloud - Stores quality information about one input point cloud.

StreamCropper::PointCloudCropper –

select_pointcloud::CandidatePointCloud

projHelper