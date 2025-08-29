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
        in {
          default = pkgs.stdenv.mkDerivation {
            pname = "roofer";
            version = "1.0.0-beta.5";

            src = ./.;

            nativeBuildInputs = with pkgs; [
              cmake
              ninja
              py
              uv
              cacert
            ] ++ lib.optionals stdenv.isDarwin [ darwin.DarwinTools apple_sdk ];

            buildInputs = with pkgs; [
              # roofer deps
              cgal
              gmp
              mpfr
              boost
              eigen
              fmt

              # apps
              mimalloc
              gdal
              nlohmann_json
              LAStools

              # python tools
              geos # for shapely
            ] ++ lib.optionals stdenv.isDarwin [ apple_sdk ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              "-DRF_BUILD_APPS=ON"
              "-DRF_BUILD_BINDINGS=OFF"
              "-DRF_BUILD_TESTING=OFF"
              "-DRF_GIT_HASH=${self.dirtyShortRev}"
            ];

            preConfigure = ''
              export pybind11_DIR="$(${py}/bin/python -c "import pybind11; print(pybind11.get_cmake_dir())")"
            '';

            hardeningDisable = [ "fortify" ];

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
              ninja

              # roofer deps
              cgal
              gmp
              mpfr
              boost
              eigen
              fmt

              # apps
              mimalloc
              gdal
              nlohmann_json
              LAStools

              # python tools
              py
              uv
              geos # for shapely

              # docs
              doxygen
            ] ++ lib.optionals stdenv.isDarwin [ darwin.DarwinTools apple_sdk ]
              ++ lib.optionals (builtins.getEnv "GITHUB_ACTIONS" == "true") [mono]; # this is needed to make gh actions binary caching work with vcpkg

            hardeningDisable = [ "fortify" ];
            VCPKG_ROOT = "${pkgs.vcpkg}/share/vcpkg";
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
