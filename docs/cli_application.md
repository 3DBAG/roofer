# Roofer CLI App

{program}`roofer` is a command line application for generating LoD1.2, LoD1.3 and LoD2 building models from pointclouds.

With the {program}`roofer` CLI application you can:

+ Generate 3D building models for very large areas with a reasonably small memory footprint. This is achieved by processing the input data in square tiles. No pre-tiling is needed.
+ Benefit from multi-threading for faster processing.
+ Read common vector input sources and filter them based on attributes or spatial extent. This means you do not need to crop the input data beforehand if it is already in a supported format.
+ Obtain detailed information per building object about the reconstruction process and the quality of the generated models and the input point cloud.


## Usage
```{code-block} text
Automatic LoD 2.2 building reconstruction from a pointclouds

Usage:
  roofer [options] <pointcloud-path>... <polygon-source> <output-directory>
  roofer [options] (-c | --config) <config-file> [(<pointcloud-path>... <polygon-source>)] <output-directory>
  roofer -h | --help
  roofer -v | --version
```

### Positional arguments

```{option} <pointcloud-path>
Path to pointcloud file (.LAS or .LAZ) or folder that contains pointcloud files.
```

```{option} <polygon-source>
Path to roofprint polygon source. Can be an OGR supported file (eg. GPKG) or database connection string.
```

```{option} <output-directory>
Output directory. The building models will be written to a CityJSONSequence file in this directory.
```

```{include} cli-options.md
```

## Output attributes
:::{note}
:class: dropdown

```{include} output-attributes.md
```
:::

## Output format
The output of the {program}`roofer` CLI application are [CityJSONSequence](https://www.cityjson.org/cityjsonseq/) files. These are a JSON Lines files that contain a sequence of CityJSON features, each feature represents all the information for one building.

> [!TIP]
>  In the near future we expect to introduce a converter program to convert the CityJSONSequence files to other formats like GPKG, OBJ and others.

## Example config file
Below is an example of a [TOML](https://toml.io/en/) configuration file for the {program}`roofer` CLI application. It shows all the available options. Noticed that many of these options are also available as command line arguments, in case one option is provided both in the configuration file and as a command line argument, the command line argument takes precedence.

```{literalinclude} example_full.toml
:language: toml
```
