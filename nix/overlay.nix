# Keep the package and container on the same GDAL feature set. See conanfile.py.
final: prev: {
  libtiff = (prev.libtiff.override { withLerc = false; }).overrideAttrs (old: {
    buildInputs = with final; [
      xz
      zlib
      zstd
    ];
    cmakeFlags = (old.cmakeFlags or [ ]) ++ [
      "-Dcxx=OFF"
      "-Djpeg=OFF"
      "-Dwebp=OFF"
      "-Dlerc=OFF"
      "-Dlibdeflate=OFF"
    ];
  });

  libgeotiff = prev.libgeotiff.overrideAttrs (_: {
    configureFlags = [
      "--without-jpeg"
      "--with-zlib=${final.zlib.dev}"
    ];
  });

  gdal =
    (prev.gdal.override {
      useMinimalFeatures = true;
      usePostgres = true;
    }).overrideAttrs
      (_: {
        pname = "gdal-roofer";
        outputs = [
          "out"
          "dev"
        ];
        nativeBuildInputs = with final; [
          cmake
          pkg-config
          bison
          python3
        ];
        buildInputs = with final; [
          proj
          geos
          sqlite
          libpq
          libtiff
          libgeotiff
          json_c
          zlib
          xz
          zstd
          lz4
        ];
        # Nixpkgs' minimal preset still enables bindings and numerous libraries.
        # Use an explicit allowlist so incidental build inputs cannot enable more.
        cmakeFlags = [
          "-DBUILD_TESTING=OFF"
          "-DBUILD_APPS=OFF"
          "-DBUILD_PYTHON_BINDINGS=OFF"
          "-DBUILD_JAVA_BINDINGS=OFF"
          "-DBUILD_CSHARP_BINDINGS=OFF"
          "-DGDAL_USE_EXTERNAL_LIBS=OFF"
          "-DGDAL_USE_INTERNAL_LIBS=OFF"
          "-DGDAL_USE_GEOS=ON"
          "-DGDAL_USE_SQLITE3=ON"
          "-DGDAL_USE_POSTGRESQL=ON"
          "-DGDAL_USE_TIFF=ON"
          "-DGDAL_USE_GEOTIFF=ON"
          "-DGDAL_USE_JSONC=ON"
          "-DGDAL_USE_ZLIB=ON"
          "-DGDAL_USE_LZMA=ON"
          "-DGDAL_USE_ZSTD=ON"
          "-DGDAL_USE_LZ4=ON"
          "-DGDAL_BUILD_OPTIONAL_DRIVERS=OFF"
          # Match Conan: retain OGR drivers whose dependencies are available.
          "-DOGR_BUILD_OPTIONAL_DRIVERS=ON"
          "-DGDAL_ENABLE_DRIVER_GTIFF=ON"
          "-DOGR_ENABLE_DRIVER_GPKG=ON"
          "-DOGR_ENABLE_DRIVER_SQLITE=ON"
          "-DOGR_ENABLE_DRIVER_PG=ON"
          "-DOGR_ENABLE_DRIVER_PGDUMP=ON"
        ];
        # The upstream install checks require the deliberately omitted Python bindings.
        doInstallCheck = false;
        nativeInstallCheckInputs = [ ];
        pythonPath = [ ];
        # gdal-config embeds include paths. Keep it with headers/CMake metadata
        # so the runtime output neither references dev nor pulls in a shell.
        postInstall = ''
          moveToOutput bin/gdal-config "$dev"
        '';
      });
}
