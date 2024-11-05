Roofer CLI App
==============

Usage:
::

   roofer [options] <pointcloud-path>... <polygon-source> <output-directory>
   roofer [options] (-c | --config) <config-file> [(<pointcloud-path>... <polygon-source>)] <output-directory>
   roofer -h | --help
   roofer -v | --version

Positional arguments:
   <pointcloud-path>            Path to pointcloud file (.LAS or .LAZ) or folder that contains pointcloud files.
   <polygon-source>           Path to footprint polygon source. Can be an OGR supported file (eg. GPKG) or database connection string.
   <output-directory>           Output directory.

Optional arguments:
   -h, --help                   Show this help message.
   -v, --version                Show version.
   -l, --loglevel <level>       Specify loglevel. Can be trace, debug, info, warning [default: info]
   --trace-interval <s>         Trace interval in seconds. Implies --loglevel trace [default: 10].
   -c <file>, --config <file>   TOML configuration file.
   -j <n>, --jobs <n>           Number of threads to use. [default: number of cores]
   --crop-output                Output cropped building pointclouds.
   --crop-output-all            Output files for each candidate pointcloud instead of only the optimal candidate. Implies --crop-output.
   --crop-rasters               Output rasterised crop pointclouds. Implies --crop-output.
   --index                      Output index.gpkg file with crop analytics.

   --split-cjseq                      Output CityJSONSequence file for each building [default: one file per output tile]
   --filter <str>                     Attribute filter
   --polygon-source-layer <str>       Load this layer from <polygon-source> [default: first layer]
   --force-lod11-attribute <str>      Building attribute for forcing lod11
   --srs <str>                        Override SRS for both inputs and outputs
   --box <xmin ymin xmax ymax>        Region of interest. Data outside of this region will be ignored
   --ceil_point_density <float>       Enfore this point density ceiling on each building pointcloud.
   --tilesize <x y>                   Tilesize used for output tiles
   --cellsize <float>                 Cellsize used for quick pointcloud analysis
   --id-attribute <str>               Building ID attribute

   -Rplane-detect-epsilon <float>     plane detect epsilon
   -Rplane-detect-k <int>             plane detect k
   -Rplane-detect-min-points <int>    plane detect min points
   -Rlod13-step-height <float>        Step height used for LoD1.3 generation
   -Rcomplexity-factor <float>        Complexity factor building reconstruction
   -Rlod <int>                        Which LoDs to generate, possible values: 12, 13, 22 [default: all]


Example config file
------------------------
.. literalinclude:: ../apps/roofer-app/example_full.toml
