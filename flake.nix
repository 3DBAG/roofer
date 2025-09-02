{
  description = "Development environment for Roofer";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs, ... }:
    let
      supportedSystems = [ "aarch64-darwin" "x86_64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { system = system; config.allowUnfree = true; };
          apple_sdk = pkgs.apple-sdk_15;
          py = pkgs.python313;
          shortRev = self.shortRev or self.dirtyShortRev or "unknown";
        in {
          default = pkgs.stdenv.mkDerivation {
            pname = "roofer";
            version = "1.0.0-beta.5";

            src = ./.;

            nativeBuildInputs = with pkgs; [
              cmake
            ] ++ lib.optionals stdenv.isDarwin [ darwin.DarwinTools apple_sdk ];

            buildInputs = with pkgs; [
              # core roofer deps
              cgal
              gmp
              mpfr
              boost
              eigen

              # app deps
              mimalloc
              gdal
              nlohmann_json
              LAStools
              geos
              fmt

              # python binding deps
              # py
              # python313Packages.pybind11
            ] ++ lib.optionals stdenv.isDarwin [ apple_sdk ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              "-DRF_BUILD_APPS=ON"
              # "-DRF_BUILD_BINDINGS=ON"
              "-DRF_BUILD_TESTING=OFF"
              "-DRF_GIT_HASH=${shortRev}"
            ];

            preConfigure = ''
              export pybind11_DIR="$(${py}/bin/python -c "import pybind11; print(pybind11.get_cmake_dir())")"
            '';

            meta = with pkgs.lib; {
              description = "3D building reconstruction from point clouds";
              homepage = "https://github.com/3DBAG/roofer";
              license = licenses.lgpl3;
              platforms = platforms.unix;
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
              pkgsStatic.boost
              eigen
              fmt

              # val3dity
              spdlog
              pugixml
              tclap

              # apps
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
