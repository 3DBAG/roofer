{
  description = "Development environment for Roofer";

  inputs.nixpkgs.url = "github:ylannl/nixpkgs/gdalMinimal";
  inputs.val3dity-src.url = "github:ylannl/val3dity";
  inputs.val3dity-src.flake = false;

  outputs = { self, nixpkgs, val3dity-src, ... }:
    let
      supportedSystems = [ "x86_64-darwin" "aarch64-darwin" "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { system = system; config.allowUnfree = true; };
          apple_sdk = pkgs.apple-sdk_15;
          py = pkgs.python313;
          shortRev = self.shortRev or self.dirtyShortRev or "unknown";

          val3dity = pkgs.stdenv.mkDerivation {
            pname = "val3dity";
            version = "2.5.3";
            src = val3dity-src;

            nativeBuildInputs = with pkgs; [ cmake ninja ];
            buildInputs = with pkgs; [
              cgal gmp mpfr eigen
              geos
              spdlog
              pugixml
              tclap
              boost
              nlohmann_json
            ];

            cmakeFlags = [ "-DVAL3DITY_LIBRARY=ON" "-DVAL3DITY_USE_INTERNAL_DEPS=OFF" "-G Ninja" ];
          };

          rooferDerivation = { withBindings ? false, withApps ? true }:
            pkgs.stdenv.mkDerivation ({
              pname = "roofer" + pkgs.lib.optionalString withBindings "py";
              version = "1.0.0-beta.5";

              src = ./.;

              nativeBuildInputs = with pkgs; [
                cmake
                ninja
              ] ++ lib.optionals stdenv.isDarwin [ darwin.DarwinTools apple_sdk ]
                ++ lib.optionals withBindings [ python313Packages.pybind11 ];

              buildInputs = with pkgs; [
                # core roofer deps
                cgal gmp mpfr boost eigen
                fmt
              ] ++ lib.optionals stdenv.isDarwin [ apple_sdk ]
                ++ lib.optionals withBindings [
                  py
                ]
                ++ lib.optionals withApps [
                  val3dity
                  spdlog
                  mimalloc
                  nlohmann_json
                  LAStools
                  geos
                  gdal
                ];

              cmakeFlags = [
                "-DCMAKE_BUILD_TYPE=Release"
                "-DRF_BUILD_APPS=${if withApps then "ON" else "OFF"}"
                "-DRF_BUILD_BINDINGS=${if withBindings then "ON" else "OFF"}"
                "-DRF_USE_VAL3DITY=${if withApps then "ON" else "OFF"}"
                "-DRF_BUILD_TESTING=OFF"
                "-DRF_GIT_HASH=${shortRev}"
                "-DRF_USE_CPM=OFF"
                "-DRF_USE_LOGGER_SPDLOG=ON"
                # there is no nix package for rerun_cpp atm
                "-DRF_USE_RERUN=OFF"
                "-G Ninja"
                "-DRF_BUILD_DOC_HELPER=ON"
              ];

              preConfigure = pkgs.lib.optionalString withBindings ''
                export pybind11_DIR="$(${py}/bin/python -c "import pybind11; print(pybind11.get_cmake_dir())")"
              '';

              meta = with pkgs.lib; {
                description = "3D building reconstruction from point clouds";
                homepage = "https://github.com/3DBAG/roofer";
                license = licenses.lgpl3;
                platforms = platforms.unix;
                mainProgram = "roofer";
              };
            });
        in {
          default = rooferDerivation { withApps = true; withBindings = false; };
          rooferpy = rooferDerivation { withApps = false; withBindings = true; };
        });

      dockerImage = forAllSystems (system:
        let
          pkgs = import nixpkgs { system = system; config.allowUnfree = true; };
        in {
          roofer = pkgs.dockerTools.buildImage {
            name = "roofer";
            tag = self.packages.${system}.default.version;
            copyToRoot = pkgs.buildEnv {
                name = "image-root";
                paths = [ self.packages.${system}.default ];
                pathsToLink = [ "/bin" ];
              };
            config = {
              Entrypoint = [ "roofer" ];
            };
          };
        });

      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { system = system; config.allowUnfree = true; };
          apple_sdk = pkgs.apple-sdk_15;
          py = pkgs.python313;
        in {
          default = pkgs.mkShell.override {
            # Use stdenvNoCC to avoid compiler contamination
            # stdenv = if pkgs.stdenv.isDarwin then pkgs.stdenvNoCC else pkgs.stdenv;
          } {
            buildInputs = with pkgs; [
              cmakeCurses

              # roofer core deps
              cgal
              gmp
              mpfr
              pkgsStatic.boost # need static for val3dity
              eigen
              fmt

              # val3dity
              pugixml
              tclap

              # apps
              spdlog
              mimalloc
              gdal
              nlohmann_json
              LAStools
              geos

              # python tools
              py
              uv

              # docs
              doxygen
            ] ++ lib.optionals stdenv.isDarwin [ darwin.DarwinTools apple_sdk ];

            UV_NO_BINARY = 1;
            # VCPKG_FORCE_SYSTEM_BINARIES = 1;
            scm_version = "unknown";

            shellHook = ''
              ${pkgs.lib.optionalString pkgs.stdenv.isDarwin ''
                # distribution script for macOS
                chmod +x distribution/macOS/bundle_libcxx.sh
                export PATH="$(pwd)/distribution/macOS:$PATH"
              ''}
              echo "Updating and activating python environment..."
              uv sync
              source .venv/bin/activate
              export pybind11_DIR="$(python -m pybind11 --cmakedir)"
              echo "Roofer dev shell with vcpkg is ready"
            '';
          };
        });
    };
}
