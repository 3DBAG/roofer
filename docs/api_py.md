# API Reference Python

The roofer python API allows you to use the roofer library from Python. Most notably it provides a `reconstruct()` function that takes a pointcloud and a roofprint polygon for a single building object and returns a reconstructed building model mesh.

> [!NOTE]
> Because roofer internally uses floats to represent coordinates, it is advised to translate your data to the origin prior to calling `reconstruct()` to prevent loss of precision.

See [this script](https://github.com/3DBAG/roofer/blob/develop/rooferpy/example_rooferpy.py) for a simple example of how to use the Python bindings.

The API uses plain Python sequences for geometry:

- `Point`: three numbers, `[x, y, z]`
- `PointCloud`: a sequence of `Point`
- `Ring`: a sequence of `Point`
- `Footprint`: a sequence of rings, where the first ring is the exterior ring and subsequent rings are holes
- `Mesh`: a sequence of polygons, where each polygon is a sequence of rings

```{eval-rst}
.. py:function:: reconstruct(points_roof, points_ground, footprint, cfg=ReconstructionConfig())
                 reconstruct(points_roof, footprint, cfg=ReconstructionConfig())
   :module: roofer

   Reconstruct a single building.

   The first overload uses both roof and ground points. The second overload
   reconstructs without ground points.

   :param points_roof: Building or roof points as a ``PointCloud``.
   :param points_ground: Ground points as a ``PointCloud``.
   :param footprint: Building footprint as a ``Footprint``.
   :param cfg: Reconstruction parameters.
   :type cfg: ReconstructionConfig
   :returns: Reconstructed meshes.
   :rtype: list[Mesh]

.. py:function:: triangulate_mesh(mesh)
   :module: roofer

   Triangulate a mesh.

   :param mesh: Mesh to triangulate.
   :type mesh: Mesh
   :returns: A tuple ``(vertices, faces)``, where ``vertices`` is a list of
       points and ``faces`` is a list of three vertex indices.
   :rtype: tuple[list[Point], list[list[int]]]
```

```{eval-rst}
.. autoclass:: roofer.ReconstructionConfig
    :members:
    :undoc-members:
```
