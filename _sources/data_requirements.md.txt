# Input data requirements

To achieve good results with the roofer building reconstruction software, the input data should meet the following requirements.

## Point cloud

+ Acquired through aerial scanning, preferably using lidar. Thus point clouds with only building facades eg. mobile mapping surveys are not supported.
+ Outliers should be removed before using roofer.
+ Should be classified beforehand, with at least *ground* and *building* classes (LAS classes 2 and 6 respectively).
+ Should have sufficient point density. We achieve good results with ~10 pts/m2 in the [3D BAG](https://3dbag.nl).
+ Should have good data consistency (good homogeneity in point density on all roofplanes), ie. there should be no large gaps in the point cloud.
+ Should be well aligned in X and Y with the 2D building roofprint polygons.
+ There should be some ground points around the building so that roofer can determine the ground floor elevation.
+ Pointcloud is automatically cropped to the extent of the 2D building polygon (CLI app).
+ In `.LAS` or `.LAZ` format for the CLI app.

## 2D building roofprint polygon

+ For each building you need to provide a 2D outline polygon.
+ Preferably a roofprint, since the input point cloud was also acquired from the air.
+ I should be well aligned with the point cloud.
+ Can be in any [default OGR supported format](https://gdal.org/en/latest/drivers/vector/index.html). This includes GeoPackage, Shapefile, PostGIS, etc.

Moreover all input data is assumed to be in the same SRS (Spatial Reference System). This SRS is assumed to be a projected system with coordinates in meters. The output data will also be in this SRS.
