from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps


class RooferRecipe(ConanFile):
    name = "roofer"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "build_apps":     [True, False],
        "use_spdlog":     [True, False],
        "use_val3dity":   [True, False],
        "build_bindings": [True, False],
        "build_testing":  [True, False],
    }
    default_options = {
        "build_apps":     True,
        "use_spdlog":     True,
        "use_val3dity":   False,
        "build_bindings": False,
        "build_testing":  False,
    }

    def requirements(self):
        # Core deps (always required)
        self.requires("cgal/6.1.1")
        self.requires("eigen/3.4.0")
        self.requires("fmt/11.1.3")

        if self.options.use_spdlog or self.options.use_val3dity:
            self.requires("spdlog/1.15.1")

        if self.options.build_apps:
            self.requires("geos/3.13.0", override=True)
            self.requires("gdal/3.12.1")
            self.requires("nlohmann_json/3.11.3")
            self.requires("laslib/2.0.2")
            # bshoshany-thread-pool is header-only, not yet in ConanCenter;
            # leave as CPM fallback for now (RF_USE_CPM=ON)

        if self.options.use_val3dity:
            self.requires("pugixml/1.15")
            self.requires("tclap/1.2.5")

        if self.options.build_testing:
            self.requires("catch2/3.7.1")

    def configure(self):
        # Keep GDAL close to the minimum feature set Roofer uses:
        # GeoPackage, PostgreSQL/PostGIS, and GeoTIFF.
        self.options["gdal"].with_arrow = False
        self.options["gdal"].with_curl = False
        self.options["gdal"].with_expat = False
        self.options["gdal"].with_geos = True
        self.options["gdal"].with_gif = False
        self.options["gdal"].with_hdf4 = False
        self.options["gdal"].with_hdf5 = False
        self.options["gdal"].with_jpeg = False
        self.options["gdal"].with_lerc = False
        self.options["gdal"].with_libdeflate = False
        self.options["gdal"].with_opencl = False
        self.options["gdal"].with_pg = True      # postgresql
        self.options["gdal"].with_png = False
        self.options["gdal"].with_qhull = False
        self.options["gdal"].with_sqlite3 = True
        self.options["libtiff"].jpeg = False
        self.options["gdal"].gdal_optional_drivers = False
        # Keep OGR optional drivers enabled: the PG driver depends on PGDump,
        # and ConanCenter does not expose per-driver toggles here.
        self.options["gdal"].ogr_optional_drivers = True
        self.options["gdal"].tools = False


    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["RF_USE_CPM"] = "OFF"
        tc.variables["RF_BUILD_APPS"] = "ON" if self.options.build_apps else "OFF"
        tc.variables["RF_USE_LOGGER_SPDLOG"] = "ON" if self.options.use_spdlog else "OFF"
        tc.variables["RF_USE_VAL3DITY"] = "ON" if self.options.use_val3dity else "OFF"
        tc.variables["RF_BUILD_BINDINGS"] = "ON" if self.options.build_bindings else "OFF"
        tc.variables["RF_BUILD_TESTING"] = "ON" if self.options.build_testing else "OFF"
        tc.generate()
