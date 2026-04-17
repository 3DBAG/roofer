{
  description = "Development environment for Roofer";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  # inputs.nixpkgs.url = "github:ylannl/nixpkgs/gdalMinimal";
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

          rerun-sdk = pkgs.stdenv.mkDerivation {
            pname = "rerun-sdk";
            version = "0.27.3";
            src = pkgs.fetchurl {
              url = "https://github.com/rerun-io/rerun/releases/download/0.27.3/rerun_cpp_sdk.zip";
              sha256 = "1aysn1jsl58vxaakv8j9awnnxll06ay4pwczfs8gi63w3w4yj920";
            };
            nativeBuildInputs = [ pkgs.unzip pkgs.cmake ];
            buildInputs = [ pkgs.arrow-cpp ];
            cmakeFlags = [
              "-DRERUN_DOWNLOAD_AND_BUILD_ARROW=OFF"
            ];
          };

          rooferDerivation = { withBindings ? false, withApps ? true, withRerun ? false }:
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
                  proj
                  sqlite
                ]
                ++ lib.optionals withRerun [
                  rerun
                  rerun-sdk
                ];

              cmakeFlags = [
                "-DCMAKE_BUILD_TYPE=Release"
                "-DRF_BUILD_APPS=${if withApps then "ON" else "OFF"}"
                "-DRF_BUILD_BINDINGS=${if withBindings then "ON" else "OFF"}"
                "-DRF_USE_VAL3DITY=${if withApps then "ON" else "OFF"}"
                "-DRF_BUILD_TESTING=OFF"
                "-DRF_GIT_HASH=${shortRev}"
                "-DRF_USE_CPM=OFF"
                "-DRF_USE_LOGGER_SPDLOG=${if withBindings then "OFF" else "ON"}"
                # there is no nix package for rerun_cpp atm
                "-DRF_USE_RERUN=${if withRerun then "ON" else "OFF"}"
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
          roofer-rerun = rooferDerivation { withApps = true; withRerun = true; };
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
          conan = pkgs.mkShell {
            buildInputs = with pkgs; [
              cmakeCurses
              ninja
              conan
            ] ++ lib.optionals stdenv.isDarwin [ darwin.DarwinTools apple_sdk ]
              ++ lib.optionals stdenv.isLinux [ patchelf ];

            shellHook = ''
              ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
                export CMAKE_LIBRARY_PATH="${pkgs.glibc.out}/lib''${CMAKE_LIBRARY_PATH:+:$CMAKE_LIBRARY_PATH}"
              ''}
              echo "Conan dev shell ready. Run 'conan profile detect' if you haven't set up a profile yet."
              echo ""
              echo "Conan build steps (replace Release with Debug for debug build):"
              echo "  conan install . --build=missing --output-folder=build-conan -s build_type=Release"
              echo "  cd build-conan"
              echo "  cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release"
              echo "  cmake --build ."
              echo ""
              ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
                roofer-patch() {
                  patchelf --set-interpreter /lib64/ld-linux-x86-64.so.2 "$1"
                  echo "Patched interpreter on $1"
                }
                echo "Tip: run 'roofer-patch <binary>' to fix the ELF interpreter for deployment on regular linux systems"
              ''}
            '';
          };

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
              echo "Updating and activating python environment..."
              uv sync
              source .venv/bin/activate
              export pybind11_DIR="$(python -m pybind11 --cmakedir)"
              echo "Roofer dev shell with Nix is ready"
            '';
          };
        });
    };
}
