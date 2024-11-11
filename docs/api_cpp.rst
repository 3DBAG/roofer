API Reference C++
=================

The roofer C++ API allows you to use the roofer as a C++ library. Most notably it provides a `reconstruct()` function that takes a pointcloud and a roofprint polygon for a single building object and returns a reconstructed building model mesh.

.. note::
   Because roofer internally uses floats to represent coordinates, it is advised to translate your data to the origin prior to calling `reconstruct()` to prevent loss of precision.

See `this file <https://github.com/3DBAG/roofer/blob/develop/tests/test_reconstruct_api.cpp>`_ for a short example of how to use the C++ bindings.

.. doxygennamespace:: roofer
   :members:

.. .. doxygenfunction:: triangulate_mesh

.. .. doxygenstruct:: roofer::ReconstructionConfig
..    :members:
..    :undoc-members:
