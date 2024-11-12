Getting started
===============

At this moment we do not yet provide separate pre-compiled binaries for roofer. To get started with roofer you need to either compile it from the sourcecode yourself or you can use the automatically built `Docker image <https://hub.docker.com/r/3dgi/roofer/tags>`_. Once roofer version 1.0 will be released, we expect to provide pre-compiled binaries for the most common platforms.

Compilation from source
-----------------------

To ease getting the required dependencies it is recommended to use `vcpkg <https://vcpkg.io>`_ to build **roofer**.

Follow the `vcpkg instructions <https://learn.microsoft.com/en-gb/vcpkg/get_started/get-started?pivots=shell-cmd>`_ to set it up.

After *vcpkg* is set up, set the ``VCPKG_ROOT`` environment variable to point to the directory where vcpkg is installed.

On *macOS* you need to install additional build tools:

.. code-block:: shell

   brew install autoconf autoconf-archive automake libtool
   export PATH="/opt/homebrew/opt/m4/bin:$PATH"

Clone the roofer repository and use one of the CMake presets to build the roofer.

.. code-block:: shell

   git clone https://github.com/3DBAG/roofer.git
   cd roofer
   mkdir build
   cmake --preset vcpkg-minimal -S . -B build
   cmake --build build
   # Optionally, install roofer
   cmake --install build
