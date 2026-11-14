# Upstream currently packages only its CLI and does not install the library.
# Reuse its build recipe, adding the library mode and a CMake consumer package.
upstream:
upstream.overrideAttrs (old: {
  cmakeFlags = old.cmakeFlags ++ [ "-DVAL3DITY_LIBRARY=ON" ];
  propagatedBuildInputs = old.buildInputs;
  installPhase = ''
    runHook preInstall
    install -Dm644 libval3dity.a "$out/lib/libval3dity.a"
    install -Dm644 "$src/src/val3dity.h" "$out/include/val3dity/val3dity.h"
    install -Dm644 "$src/src/definitions.h" "$out/include/val3dity/definitions.h"
    install -Dm644 ${./val3dityConfig.cmake} "$out/lib/cmake/val3dity/val3dityConfig.cmake"
    runHook postInstall
  '';
})
