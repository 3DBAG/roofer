{
  description = "Roofer packages, development shells, and container images";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  inputs.val3dity.url = "github:tudelft3d/val3dity";
  inputs.val3dity.inputs.nixpkgs.follows = "nixpkgs";

  outputs =
    {
      self,
      nixpkgs,
      val3dity,
    }:
    let
      inherit (nixpkgs) lib;
      supportedSystems = [
        "x86_64-darwin"
        "aarch64-darwin"
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = lib.genAttrs supportedSystems;
      linuxSystems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      overlay = import ./nix/overlay.nix;
      pkgsFor = forAllSystems (
        system:
        import nixpkgs {
          inherit system;
          config.allowUnfree = true;
          overlays = [ overlay ];
        }
      );
      gitRev = self.rev or self.dirtyRev or "unknown";
      src = lib.fileset.toSource {
        root = ./.;
        fileset = lib.fileset.unions [
          ./CMakeLists.txt
          ./apps
          ./cmake
          ./include
          ./src
          ./rooferpy
          ./LICENSE
          ./README.md
        ];
      };
    in
    {
      overlays.default = overlay;

      packages = forAllSystems (
        system:
        let
          pkgs = pkgsFor.${system};
          roofer = pkgs.callPackage ./nix/package.nix {
            inherit src gitRev;
            val3dity = import ./nix/val3dity-library.nix val3dity.packages.${system}.default;
            rerun-sdk = pkgs.callPackage ./nix/rerun-sdk.nix { };
          };
        in
        {
          default = roofer;
          roofer-val3dity = roofer.override { withVal3dity = true; };
          rooferpy = roofer.override {
            withApps = false;
            withBindings = true;
          };
          docs = roofer.override {
            withApps = false;
            withBindings = true;
            withDocHelper = true;
          };
          roofer-rerun = roofer.override { withRerun = true; };
        }
      );

      checks = forAllSystems (
        system:
        let
          pkgs = pkgsFor.${system};
        in
        {
          gdal =
            pkgs.runCommand "roofer-gdal-check"
              {
                nativeBuildInputs = [
                  pkgs.stdenv.cc
                  pkgs.pkg-config
                ];
                buildInputs = [ pkgs.gdal ];
              }
              ''
                $CXX ${./nix/check-gdal.cpp} $(pkg-config --cflags --libs gdal) -o check-gdal
                ./check-gdal
                touch $out
              '';
        }
      );

      dockerImage = lib.genAttrs linuxSystems (
        system:
        let
          pkgs = pkgsFor.${system};
          roofer = self.packages.${system}.default;
        in
        {
          roofer = pkgs.dockerTools.buildLayeredImage {
            name = "roofer";
            tag = roofer.version;
            contents = [
              roofer
              pkgs.cacert
              pkgs.dockerTools.fakeNss
            ];
            extraCommands = "mkdir -m 1777 tmp";
            config = {
              Entrypoint = [ "${roofer}/bin/roofer" ];
              WorkingDir = "/data";
              Env = [ "SSL_CERT_FILE=${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt" ];
            };
          };
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = pkgsFor.${system};
        in
        {
          conan = pkgs.mkShell {
            packages =
              with pkgs;
              [
                cmakeCurses
                ninja
                conan
              ]
              ++ lib.optionals stdenv.isDarwin [
                darwin.DarwinTools
                apple-sdk_15
              ];
            shellHook = ''
              ${lib.optionalString pkgs.stdenv.isLinux ''
                export CMAKE_LIBRARY_PATH="${pkgs.glibc.out}/lib''${CMAKE_LIBRARY_PATH:+:$CMAKE_LIBRARY_PATH}"
              ''}
              echo "Conan dev shell ready. See README-dev.md for build instructions."
            '';
          };

          default = pkgs.mkShell {
            inputsFrom = [
              self.packages.${system}.default
              self.packages.${system}.rooferpy
            ];
            packages = with pkgs; [
              cmakeCurses
              gnumake
              catch2_3
              uv
              llvmPackages_18.clang-tools
              doxygen
            ];
            UV_NO_BINARY = 1;
            shellHook = ''
              roofer-python-setup() {
                uv sync --frozen --python ${pkgs.python313}/bin/python || return
                source .venv/bin/activate || return
                local pybind11_dir
                pybind11_dir="$(python -m pybind11 --cmakedir)" || return
                export pybind11_DIR="$pybind11_dir"
              }
              echo "Roofer dev shell ready. Run roofer-python-setup to sync and activate Python dependencies."
            '';
          };
        }
      );
    };
}
