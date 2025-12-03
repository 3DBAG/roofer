# roofer - automatic building reconstruction from pointclouds

**roofer** performs fully automatic LoD2 building reconstruction from a pointcloud and a building roofprint polygon. It was originally developed, and is still used, to create the [3DBAG dataset](https://3dbag.nl). To use it, read [how to get started](https://3dbag.github.io/roofer/getting_started.html).

![Building reconstruction with roofer](https://raw.githubusercontent.com/3DBAG/roofer/refs/heads/develop/docs/_static/img/banner.png)

## Highlights

- It reconstructs a 3D building model from a pointcloud and a 2D roofprint polygon
- It is a fully automated process, there is no manual modelling required.
- It is possible to tweak the reconstruction parameters to adjust it a little to different input data qualities.
- It can output different Level of Details: LoD1.2, LoD1.3, LoD2.2. See [the refined Level of Details by the 3D geoinformation research group](https://3d.bk.tudelft.nl/lod/).
- Usable either as a command line application or as a library with C++ and Python bindings.
- With the CLI application the building models are outputted to a [CityJSONSequence](https://www.cityjson.org/cityjsonseq/) file.

## Documentation and Source Code

Documentation is provided at [https://3dbag.github.io/roofer/](https://3dbag.github.io/roofer/)
The source code is available at [https://github.com/3DBAG/roofer](https://github.com/3DBAG/roofer)

> [!CAUTION]
> Roofer is currently still under active development and subject to breaking changes.

## Origin

The building reconstruction algorithms underpinning roofer were originally developed within the [3D geoinformation research group](https://3d.bk.tudelft.nl/) at the Technical University of Delft.
From 2022 onwards, [3DGI](https://3dgi.nl), a spinoff company from the aforementioned research group, joined the development efforts.

This project has received funding from the European Research Council (ERC):
- *2016-2022* under the European Unions Horizon2020 Research & Innovation Programme (grant agreement no. 677312 UMnD: Urban modelling in higher dimensions).
- *2022-2024* under the Horizon Europe Research & Innovation Programme (grant agreement no. 101068452 3DBAG: detailed 3D Building models Automatically Generated for very large areas).

In *2024* this project has received funding from Kadaster, the Netherlands' Cadastre, Land Registry and Mapping Agency.

Prior to 2024 the building reconstruction algorithms were developed as part of *Geoflow* and the *gfp-building-reconstruction plugin*. During the summer of 2024 the code was refactored and the *roofer* project was born.

## License

Roofer is licensed under the GPLv3 license.

## 3DBAG organisation

Roofer is part of the 3DBAG organisation. For more information visit the [3DBAG organisation](https://github.com/3DBAG) on GitHub.
