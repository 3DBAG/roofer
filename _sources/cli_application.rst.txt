Roofer CLI App
==============

Usage:
::

  roofer -c <file>

Options:
  -h, --help                   Show this help message.
  -V, --version                Show version.
  -v, --verbose                Be more verbose.
  -t, --trace                  Trace the progress. Implies --verbose.
  --trace-interval <s>         Trace interval in seconds [default: 10].
  -c <file>, --config <file>   Config file.
  -j <n>, --jobs <n>           Limit the number of threads used by roofer. Default is to use all available resources.

Minimum example config file
---------------------------
.. literalinclude:: ../apps/roofer-app/example_min.toml

Full example config file
------------------------
.. literalinclude:: ../apps/roofer-app/example_full.toml
