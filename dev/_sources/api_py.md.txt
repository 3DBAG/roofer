# API Reference Python

The roofer python API allows you to use the roofer library from Python. Most notably it provides a `reconstruct()` function that takes a pointcloud and a roofprint polygon for a single building object and returns a reconstructed building model mesh.

> [!NOTE]
> Because roofer internally uses floats to represent coordinates, it is advised to translate your data to the origin prior to calling `reconstruct()` to prevent loss of precision.

See [this script](https://github.com/3DBAG/roofer/blob/develop/rooferpy/example_rooferpy.py) for a simple example of how to use the Python bindings.

```{eval-rst}  
.. automodule:: roofer
:members:
:undoc-members:
```
