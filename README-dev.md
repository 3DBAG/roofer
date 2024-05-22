# Developing roofer

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

Various test cases are available at https://data.3dgi.xyz/roofer-test-data.
Each test case is documented in the README.

The test data is downloaded automatically into `tests/data` during the project configuration, provided that you enable the tests with `RF_ENABLE_TESTING`.
Once available locally, the data is not re-downloaded, unless the files are removed or they change on the server.

#### Adding test data

The data for tests is stored at [https://data.3dgi.xyz/roofer-test-data](https://data.3dgi.xyz/roofer-test-data). To add new data, upload a zip of the data files only. The toml configuration is checked into git and placed into `tests/config`. Make sure to use consistent names for the data files and tests.

See `tests/CMakeLists.txt` how to fetch the data from the server and make it available for the tests. Note that `FetchContent` extracts the zip contents recursively. Thus, specify the directory as for instance `SOURCE_DIR "${DATA_DIR}/wippolder"` to have the contents placed into `data/wippolder`.

## Documentation

Dependencies:
- [Doxygen](https://www.doxygen.nl/index.html)
- [graphviz](https://www.graphviz.org) for the graphs (using `dot`)

To build the documentation locally, run doxygen from the `docs` directory.

```shell
cd docs
doxygen
```

The rendered documentation is in the `docs/html` directory, and the main page is `docs/html/index.html`.

#### Documenting the code

This library uses the Javadoc style to document the code.
That is the comment block starts with two *'s.

Full [Doxygen documentation](https://www.doxygen.nl/manual/docblocks.html#specialblock).
