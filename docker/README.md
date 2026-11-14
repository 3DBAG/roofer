The Docker image is built directly from the default flake package and its Nix
runtime closure, without a base distribution or a second dependency recipe.
The documentation helper is available separately in the `docs` flake package and
is omitted from the default package and image.

```sh
# Use aarch64-linux on an ARM64 Linux builder.
nix build .#dockerImage.x86_64-linux.roofer --out-link image
docker load --input image
docker run --rm -v "$PWD:/data" "roofer:$(nix eval --raw .#default.version)" --help
```

Run with the same CLI arguments as the installed `roofer` package. The working
directory is `/data`; mount input/output files there. The image has no base
distribution, package manager, or Nix build environment. `/tmp` is writable, and
CA certificates are included for TLS connections.

GDAL is configured in `nix/overlay.nix`, shared by the flake packages and development
shell. It retains GeoPackage, PostgreSQL/PostGIS, GeoTIFF, GEOS, and OGR drivers
whose dependencies are available (including GeoJSON and Shapefile), following
`conanfile.py`. GDAL bindings, CLI tools, and unused format libraries are omitted;
TIFF JPEG, WebP, and LERC codecs are disabled. PROJ's database and GDAL resources
remain in the runtime closure. The default package omits optional val3dity
validation, matching the previous Docker build; use `nix build .#roofer-val3dity`
for a package with validation enabled.

```sh
nix build .#checks.x86_64-linux.gdal --no-link
nix path-info --closure-size --human-readable .#default
du -hL image  # compressed Docker archive; docker image inspect reports unpacked size
```
