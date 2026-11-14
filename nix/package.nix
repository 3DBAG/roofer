{
  lib,
  stdenv,
  cmake,
  ninja,
  cgal,
  gmp,
  mpfr,
  boost,
  eigen,
  fmt,
  spdlog,
  nlohmann_json,
  LAStools,
  geos,
  gdal,
  proj,
  sqlite,
  python313,
  python313Packages,
  darwin,
  apple-sdk_15,
  src,
  gitRev,
  val3dity,
  rerun,
  rerun-sdk,
  withApps ? true,
  withBindings ? false,
  withVal3dity ? false,
  withRerun ? false,
  withDocHelper ? false,
}:
stdenv.mkDerivation {
  pname = if withDocHelper then "roofer-docs" else "roofer" + lib.optionalString withBindings "py";
  version = "1.1.0-beta.1";
  inherit src;

  nativeBuildInputs = [
    cmake
    ninja
  ]
  ++ lib.optionals stdenv.isDarwin [
    darwin.DarwinTools
    apple-sdk_15
  ]
  ++ lib.optionals withBindings [ python313Packages.pybind11 ];

  buildInputs = [
    cgal
    gmp
    mpfr
    boost
    eigen
  ]
  ++ lib.optionals (withApps || withDocHelper) [ fmt ]
  ++ lib.optionals withVal3dity [ val3dity ]
  ++ lib.optionals stdenv.isDarwin [ apple-sdk_15 ]
  ++ lib.optionals withBindings [ python313 ]
  ++ lib.optionals withApps [
    spdlog
    nlohmann_json
    LAStools
    geos
    gdal
    proj
    sqlite
  ]
  ++ lib.optionals withRerun [
    rerun
    rerun-sdk
  ];

  cmakeFlags = [
    (lib.cmakeFeature "CMAKE_BUILD_TYPE" "Release")
    (lib.cmakeFeature "RF_GIT_HASH" gitRev)
    (lib.cmakeBool "RF_BUILD_APPS" withApps)
    (lib.cmakeBool "RF_BUILD_BINDINGS" withBindings)
    (lib.cmakeBool "RF_USE_VAL3DITY" withVal3dity)
    (lib.cmakeBool "RF_BUILD_TESTING" false)
    (lib.cmakeBool "RF_USE_CPM" false)
    (lib.cmakeBool "RF_USE_LOGGER_SPDLOG" (!withBindings))
    (lib.cmakeBool "RF_USE_RERUN" withRerun)
    (lib.cmakeBool "RF_BUILD_DOC_HELPER" withDocHelper)
  ];

  preConfigure = lib.optionalString withBindings ''
    export pybind11_DIR="$(${python313}/bin/python -m pybind11 --cmakedir)"
  '';

  meta = {
    description = "3D building reconstruction from point clouds";
    homepage = "https://github.com/3DBAG/roofer";
    license = lib.licenses.lgpl3;
    platforms = lib.platforms.unix;
  }
  // lib.optionalAttrs (withApps || withDocHelper) {
    mainProgram = if withApps then "roofer" else "doc-helper";
  };
}
