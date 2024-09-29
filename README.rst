
roofer - automatic building reconstruction from pointclouds
===========================================================

**roofer** performs fully automatic LoD2 building reconstruction from a pointcloud and a building footprint polygon. It was originally developed, and is still used, for the `3DBAG project <https://3dbag.nl>`_.

.. image:: https://raw.githubusercontent.com/3DBAG/roofer/refs/heads/develop/docs/_static/img/banner.png
  :width: 800
  :alt: Building reconstruction with roofer

Highlights:

+ It reconstructs a 3D building model from a pointcloud and a 2D footprint polygon
+ It is a fully automated process, there is no manual modelling required.
+ It is possible to tweak the reconstruction parameters to adjust it a little to different input data qualities.
+ It can output different Level of Details: LoD1.2, LoD1.3, LoD2.2. See `the refined Levef of Details by the 3D geoinformation research group <https://3d.bk.tudelft.nl/lod/>`_.
+ Usable either as a command line application or as a library with C++ and Python bindings.
+ With the CLI application the building models are outputted to a `CityJSONSequence <https://www.cityjson.org/cityjsonseq/>`_ file.

Documentation is provided at https://3dbag.github.io/roofer/.
