{
  description = "Development environment for Roofer";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      supportedSystems = [ "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          apple_sdk = pkgs.apple-sdk_15;
          py = pkgs.python313;
        in {
          default = pkgs.mkShell {
            buildInputs = with pkgs; [
              cmakeCurses
              vcpkg
              ninja
              llvmPackages.libcxxClang

              # to make vcpkg work
              autoconf
              automake
              autoconf-archive
              pkg-config-unwrapped
              bash
              cacert
              coreutils
              curl
              gnumake
              gzip
              openssh
              perl
              pkg-config
              libtool
              zip
              zstd

              # python tools
              py
              uv

              # docs
              doxygen
            ] ++ lib.optionals stdenv.isDarwin [ darwin.DarwinTools apple_sdk ];

            hardeningDisable = [ "fortify" ];
            VCPKG_ROOT = "${pkgs.vcpkg}/share/vcpkg";
            VCPKG_FORCE_SYSTEM_BINARIES = 1;

            shellHook = ''
              echo "Updating and activating python environment..."
              uv sync
              source .venv/bin/activate
              export pybind11_ROOT="$(python -m pybind11 --cmakedir)"
              echo "Roofer dev shell with vcpkg is ready"
            '';
          };
        });
    };
}
