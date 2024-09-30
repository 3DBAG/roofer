Getting started
===============

Compilation from source
-----------------------

It is recommended to use `vcpkg <https://vcpkg.io>`_ to build **roofer**.

Follow the `vcpkg instructions <https://learn.microsoft.com/en-gb/vcpkg/get_started/get-started?pivots=shell-cmd>`_ to set it up.

After *vcpkg* is set up, set the ``VCPKG_ROOT`` environment variable to point to the directory where vcpkg is installed.

On *macOS* you need to install additional build tools:

.. code-block:: shell

   brew install autoconf autoconf-archive automake libtool
   export PATH="/opt/homebrew/opt/m4/bin:$PATH"

Clone this repository and use one of the CMake presets to build the roofer.

.. code-block:: shell

   cd roofer
   mkdir build
   cmake --preset vcpkg-minimal -S . -B build
   cmake --build build
   # Optionally, install roofer
   cmake --install build

Requirements on the input data
------------------------------

Point cloud
^^^^^^^^^^^

+ Acquired through aerial scanning, either Lidar or Dense Image Matching. But Lidar is preferred, because it is often of higher quality. Thus point clouds with only building facades eg. mobile mapping surveys are not supported.
+ The fewer outliers the better.
+ Classified, with at least *ground* and *building* classes.
+ Has sufficient point density. We achieve good results with 8-10 pts/m2 in the `3D BAG <https://3dbag.nl>`_.
+ Well aligned with the 2D building polygon.
+ Do include some ground points around the building so that the software can determine the ground floor elevation.
+ Pointcloud is automatically cropped to the extent of the 2D building polygon.
+ In `.LAS` or `.LAZ` format for the CLI app.

2D building polygon
^^^^^^^^^^^^^^^^^^^

+ A simple 2D polygon of a single building.
+ Preferably roofprint, since the input point cloud was also acquired from the air.
+ Well aligned with the point cloud.
+ In GeoPackage or ESRI Shapefile format, or a PostGIS database connection.
