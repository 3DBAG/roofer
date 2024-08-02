# roofer: Automatic 3D building reconstruction

## What does it do

Fully automatic LoD2 building reconstruction from a classified LiDAR point cloud and a set of well aligned building footprints.

## History
Most of the building reconstruction algorithm was initially developed within the wider Geoflow framework. This was good for prototyping and development, but the complexity of the framework made it increasingly harder to run and maintain in practice. Also the code was spread in multiple repositories that made it cumbersome to do version management. Hence the need for a simpler and dedicated software that focuses just on building reconstruction.

### Naming
Since the name geoflow got (exclusively) known for the building reconstruction algorithm (which was technically just a part of geoflow and did not have a proper name itself), it may be better to keep the name geoflow for the building reconstruction method itself. The name roofer may be more fitting to a building reconstruction algo, but geoflow is preferred for consistency and to avoid confusion to outsiders.

## Goals/use cases
The goal with roofer is to deliver something that is useful for

1. Researchers that want to experiment and/or improve the building reconstruction algorithm
2. Maintaining the 3DBAG dataset
3. Reliable building reconstruction for large scale projects apart from 3DBAG

## Requirements
* Minimum number of preferably lightweight dependencies. Non essential dependencies should be optional.
* easy to compile and install on all major platforms.
* Uncomplicated and straightforward to use, both API and apps
* Building reconstruction pipeline should be easy to adapt and extend (to support and encourage the exploration of new reconstruction algorithms)
* Flexible logging framework that allows both text and geometry logging. Must enable easy visual debugging with tools like Rerun (within generic logging framework or not?).
* Each input building must generate an output (app), even if reconstruction was not successful. If reconstruction failed for a building it must be clear why (app+api).
* Integrate well with CityJSON ecosystem. Specifically output of reconstruct app.
* Easy to run both a single building or a large set of buildings. In case of large set, it should also be easy to pick out and inspect one building or a bbox subset (for debugging or parameter tuning).
* Fast reconstruction of large number of buildings. Focus on I/O efficiency and implement parallel processing where it makes sense.

## Scope/Limitations
* Roofer outputs CityJSON features, other formats can be obtained through CityJSON conversion tools
* Atm we do not do automatic footprint extraction from the pointcloud, although this might be added in the future.
* for now focus on CLI only. GUI maybe later.
* Coordinate transformation ??
* geometry validation (only inside reconstruction code for debugging/testing purposes, not for 'production') ??

* assumption: projected coordinates in unit of meters.
* limitation: flaws in the point cloud (occlusions, low pt density) may lead to issues with reconstruction

## Overview

Roofer consists of (1) a software library that also includes an API for developers (2) a set of command line applications that allows one to perform building reconstruction in practice, in a straightforward and efficient manner.

### Library components
* reconstruction - core building reconstruction functions
* IO - input/output handling
* datastructures - common datastructures
* misc - miscellaneous functions

### apps

* crop - crops point cloud for each building
  * Reads LAS files and list of building footprint vectors
  * Outputs reconstruct input for each building

* reconstruct - performs reconstruction per single building
  * outputs a CityJSON feature per building

## Architecture

### crop + reconstruct pipeline
For processing efficiently large number of buildings.

Input: folder of LAS/LAZ files + vector file with footprints

`crop` should be able to read arbitrarily large inputs without running out of memory. Strategy is to
1. build a spatial of index input files
2. Stream read the points and accumulate them in the building footprints.
3. Once all points for a building have been read we can output that building and release the memory it occupied.

this should build further on how the StreamCropper currently already works. Initially implemented on one thread, multithreading can be added later if it makes sense. Output can be written to disk or streamed directly into the reconstruct app. Preferably this is done in a way with minimal overhead due to encoding/decoding.

Q. how to deal with change detection/multiple pc selection?

`reconstruct` uses a thread pool/co-routines (?) to efficiently reconstruct all input buildings. Output is written to disk in a binary CityJSON format.

output of reconstruct could be streamed to other compatible applications for format conversion, enrichment with eg party walls, tiling etc.

### roofer (crop + reconstruct)

#### Multithreading

The `roofer` process is split into four stages. Each stage runs on its own thread, processing data as soon as it receives it. Thus, once the cropper is done with the first tile, the reconstruction of that tile begins immediately, followed by sorting and serialization.

- crop (serial)
- reconstruct (parallel)
- sort (serial)
- serialize (serial)

Thread synchronization is managed with mutexes and condition variables.
Data is moved from one deque to another among the stages and finally deleted after it is serialized.

Cropping is a serial process that produces one `BuildingTile` after another.

The reconstructor thread pushes the buildings of a tile onto the reconstruction queue, and each building is submitted as a task onto the reconstruction-thread-pool. The threads are detached, and they run as long as the cropper is running or there are cropped buildings. When a building is finished, the corresponding `Progress` enum is set to `RECONSTRUCTION_SUCCEEDED` or `RECONSTRUCTION_FAILED`.

The reconstructed buildings are finished in random order, so the sorter thread collects the finished (failed or succeeded) buildings of a tile, and place them back into the tile in their original order. A tile is finished when all of its buildings are either `RECONSTRUCTION_SUCCEEDED` or `RECONSTRUCTION_FAILED`.

The complete tiles are serialized by the serializer thread and finally released from memory.

### Datastructures
Simple and easy to use types to handle vector geometries (pointcloud, polygons, meshes), simple rasters, nullable attributes (int, float, bool, string, date/time) + common operations on those types.

* Geometry types should not inherit from std::vector, this is not good practice
* should however implement a similar interface to std::vector, with size(), begin(), end(), front(), back(), operator[]() etc
* use contiguous memory for coordinates
* maybe make number type (float, double) a template parameter (???)
* name the geometry types in accordance with CityJSON? or WKT? (poincloud, segments, linestring, surface (polygon), solid)
* how to handle semantics for solids?

* AttributeVecMap already has most of the needed functionality
* maybe the usability could be improved, atm some of the code that used AVM is quite verbose
* it is not enforced that all attribute vectors in the same map have the same length. Is this a big problem? we could just add a function that checks this and is called at critical moments in the code
* would AVM work well with streaming features in crop?

* Raster?

### Binary CityJSON
For efficient streaming processing of large number of building features.

* focus on fast read and write, storage size/compression is not a priority at this time
* compatible with CityJSON Sequence spec
* store many features in one large binary file
* should be possible to add (spatial) indices
