Roofer CLI App
==============

:program:`roofer` is a command line application for generating LoD1.2, LoD1.3 and LoD2 building models from pointclouds.

Usage
-----
.. code-block:: text

  roofer [options] <pointcloud-path>... <polygon-source> <output-directory>
  roofer [options] (-c | --config) <config-file>
                  [(<pointcloud-path>... <polygon-source>)] <output-directory>
  roofer -h | --help
  roofer -v | --version

Options
^^^^^^^

.. option:: <pointcloud-path>

  Path to pointcloud file (.LAS or .LAZ) or folder that contains pointcloud files.

.. option:: <polygon-source>

  Path to footprint polygon source. Can be an OGR supported file (eg. GPKG) or database connection string.

.. option:: <output-directory>

  Output directory.


.. option:: -h, --help

  Show this help message.

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

  Attribute filter

.. option:: --polygon-source-layer <str>

  Load this layer from <polygon-source> [default: first layer]

.. option:: --force-lod11-attribute <str>

  Building attribute for forcing lod11

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

  plane detect epsilon

.. option:: -Rplane-detect-k <int>

  plane detect k

.. option:: -Rplane-detect-min-points <int>

  plane detect min points

.. option:: -Rlod13-step-height <float>

  Step height used for LoD1.3 generation

.. option:: -Rcomplexity-factor <float>

  Complexity factor building reconstruction

.. option:: -Rlod <int>

  Which LoDs to generate, possible values: 12, 13, 22 [default: all]



Example config file
-------------------
.. literalinclude:: ../apps/roofer-app/example_full.toml
