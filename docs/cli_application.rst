Roofer CLI App
==============

:program:`roofer` is a command line application for generating LoD1.2, LoD1.3 and LoD2 building models from pointclouds.

With the :program:`roofer` CLI application you can:

+ Generate of 3D building models for very large areas with a reasonably small memory footprint. This is achieved by processing the input data in square tiles. No pre-tiling is needed.
+ Benefit from multi-threading for faster processing.
+ Read common vector input sources and filter them based on attributes or spatial extent. This means you do not need to crop the input data beforehand if it is already in a supported format.
+ Obtain detailed information per building object about the reconstruction process and the quality of the generated models and the input point cloud.

Usage
-----
.. code-block:: text

  roofer [options] <pointcloud-path>... <polygon-source> <output-directory>
  roofer [options] (-c | --config) <config-file>
                  [(<pointcloud-path>... <polygon-source>)] [<output-directory>]
  roofer -h | --help
  roofer -v | --version

Options
^^^^^^^

.. option:: <pointcloud-path>

  Path to pointcloud file (.LAS or .LAZ) or folder that contains pointcloud files.

.. option:: <polygon-source>

  Path to roofprint polygon source. Can be an OGR supported file (eg. GPKG) or database connection string.

.. option:: <output-directory>

  Output directory. The building models will be written to a CityJSONSequence file in this directory.

.. option:: -h, --help

  Show help message.

.. option:: -v, --version

  Show version.

.. option:: -l, --loglevel <level>

  Specify loglevel. Can be trace, debug, info, warning [default: info]

.. option:: --trace-interval <s>

  Trace interval in seconds. Implies :option:`--loglevel` trace [default: 10].

.. option:: -c <file>, --config <file>

  TOML configuration file.

.. option:: -j <n>, --jobs <n>

  Number of threads to use. [default: number of cores]

.. option:: --crop-output

  Output cropped building pointclouds.

.. option:: --crop-output-all

  Output files for each candidate pointcloud instead of only the optimal candidate. Implies :option:`--crop-output`.

.. option:: --crop-rasters

  Output rasterised crop pointclouds. Implies :option:`--crop-output`.

.. option:: --index

  Output index.gpkg file with crop analytics.


.. option:: --split-cjseq

  Output CityJSONSequence file for each building [default: one file per output tile]

.. option:: --filter <str>

  Specify WHERE clause in OGR SQL to select specfic features from <polygon-source>

.. option:: --polygon-source-layer <str>

  Load this layer from <polygon-source> [default: first layer]

.. option:: --force-lod11-attribute <str>

  Building attribute for forcing lod11.

.. option:: --srs <str>

  Override SRS for both inputs and outputs

.. option:: --box <xmin ymin xmax ymax>

  Region of interest. Data outside of this region will be ignored

.. option:: --ceil_point_density <float>

  Enfore this point density ceiling on each building pointcloud.

.. option:: --tilesize <x y>

  Tilesize used for output tiles

.. option:: --cellsize <float>

  Cellsize used for quick pointcloud analysis

.. option:: --id-attribute <str>

  Building ID attribute





.. option:: -Rplane-detect-epsilon <float>

  See :cpp:member:`roofer::ReconstructionConfig::plane_detect_epsilon`.

.. option:: -Rplane-detect-k <int>

  See :cpp:member:`roofer::ReconstructionConfig::plane_detect_k`.

.. option:: -Rplane-detect-min-points <int>

  See :cpp:member:`roofer::ReconstructionConfig::plane_detect_min_points`.

.. option:: -Rlod13-step-height <float>

  See :cpp:member:`roofer::ReconstructionConfig::lod13_step_height`.

.. option:: -Rcomplexity-factor <float>

  See :cpp:member:`roofer::ReconstructionConfig::complexity_factor`.

.. option:: -Rlod <int>

  See :cpp:member:`roofer::ReconstructionConfig::lod`. Default is to reconstruct all LoDs.

Output format
-------------
The output of the :program:`roofer` CLI application are `CityJSONSequence <https://www.cityjson.org/cityjsonseq/>`_ files. These are a JSON Lines files that contain a sequence of CityJSON features, each feature represents all the information for one building.

..  info::
  In the near future we expect to introduce a converter program to convert the CityJSONSequence files to other formats like GPKG, OBJ and others.

Example config file
-------------------
Below is an example of a `TOML <https://toml.io/en/>` configuration file for the :program:`roofer` CLI application. It shows all the available options. Noticed that many of these options are also available as command line arguments, incase one option is provided both in the configuration file and as a command line argument, the command line argument takes precedence.

.. literalinclude:: ../apps/roofer-app/example_full.toml
