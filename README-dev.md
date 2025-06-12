# Developing roofer

## Formatting and static analysis with Clang Tools

clang-tidy is part of the Extra Clang Tools, clang-format is part of the Core Clang Tools, see [link](https://clang.llvm.org/docs/ClangTools.html).

The formatting rules are configured with the `.clang-format` file.
Formatting is enforced in the CI checks.
Therefore, it is recommended that you automate the code formatting in some way.
Your code editor might also have a clang-format integration and able to format the code based on the `.clang-format` config file.
You can also set up a git hook to do the formatting for you on each commit.
See the `Pre-commit hook` section below.

The linter checks are configured with the `.clang-tidy` file.

## Pre-commit hook for Git (GitHub)

[pre-commit](https://pre-commit.com/) is tool that can install git commit hooks locally and there is also a corresponding GitHub Action.
The commit hooks are configured in `.pre-commit-config.yaml`.

The `pre-commit` GitHub Action and `.pre-commit-config.yaml` configuration runs checks the changed lines with clang-format and reports if there is anything problematic with the formatting.

To run pre-commit manually:

```
pip install -r requirements.txt
pre-commit run -a
```

## Testing

Tests are run with `CTest`, which either runs the test executables directly, or delegates testing to `pytest`.
`pytest` is used for testing the installed artifacts.

See the CMake presets for the available test configurations.

### Integration

- The preset `test-built` tests the compiled executables, in their build directory.

    ```ctest --preset test-built```

- The preset `test-installed` tests the installed executables, by calling the exe in a subprocess from python, using pytest.

    ```ctest --preset test-installed```

### Test data

Various test cases are available at https://data.3dbag.nl/testdata/roofer.
Each test case is documented in the README.

The test data is downloaded automatically into `tests/data` during the project configuration, provided that you enable the tests with `RF_BUILD_TESTING`.
Once available locally, the data is not re-downloaded, unless the files are removed or they change on the server.

#### Adding test data

The data for tests is stored at [https://data.3dbag.nl/testdata/roofer](https://data.3dbag.nl/testdata/roofer). To add new data, upload a zip of the data files only. The toml configuration is checked into git and placed into `tests/config`. Make sure to use consistent names for the data files and tests.

See `tests/CMakeLists.txt` how to fetch the data from the server and make it available for the tests. Note that `FetchContent` extracts the zip contents recursively. Thus, specify the directory as for instance `SOURCE_DIR "${DATA_DIR}/wippolder"` to have the contents placed into `data/wippolder`.
Pass the md5 hash of the zipfile as the `URL_HASH` to `FetchContent`, so that only changed archives are re-downloaded.

**Example (wippolder test case)**

The input data directory `tests/data/wippolder` has the following structure:

```
wippolder/
├── LICENSE
├── wippolder.gpkg
├── wippolder.las
└── wippolder.txt
```

The `tests/data/wippolder/wippolder.txt` contains the WKT of the area.

The ZIP compressed `tests/data/wippolder` directory is uploaded to `https://data.3dbag.nl/testdata/roofer`.
The data is declared in `tests/CMakeLists.txt` with `FetchContent`.
The archive's MD5 has is `8efc0a7cfb48d3d17f1dc834e3350efe`.

```cmake
# tests/CMakeLists.txt
FetchContent_Declare(
  wippolder
  URL "${DATA_URL_ROOT}/wippolder.zip"
  URL_HASH MD5=8efc0a7cfb48d3d17f1dc834e3350efe
  SOURCE_DIR "${DATA_DIR}/wippolder")
FetchContent_MakeAvailable(wippolder)
```

The `tests/README.md` contains the detailed test data description.

The `tests/config/*-wippolder.toml` configuration files refer to the input data directory, relative to the `tests` directory.

```toml
[input.footprint]
path = "data/wippolder/wippolder.gpkg"
```

Finally, tests use the configuration file , e.g. the test that runs `roofer` with the `wippolder` input, uses `roofer-wippolder.toml`.

```cmake
# tests/CMakeLists.txt
add_test(
  NAME "roofer-wippolder"
  COMMAND $<TARGET_FILE:roofer> --config "${CONFIG_DIR}/roofer-wippolder.toml"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
```

## Documentation

To build the documentation locally:

```shell
cd docs
make html
```

The rendered documentation is in the `docs/html` directory, and the main page is `docs/html/index.html`.

#### Documenting the code

This library uses the Javadoc style to document the code.
That is the comment block starts with two *'s.

Full [Doxygen documentation](https://www.doxygen.nl/manual/docblocks.html#specialblock).

## Version bumping
Use bump-my-version.

To check the available bump options:

```shell
bump-my-version show-bump
```

To bump eg. the beta counter:
```shell
bump-my-version bump beta
```
Tip: first run with `--dry-run -v` to check what would change before applying the changes.
