# Developing roofer

## Testing

Tests are run with `CTest`.
See the CMake presets for the test configurations.

```shell
ctest
```

### Integration

Integration tests are run with CTest and pytest.
`CTest` is used for testing the compiled executables, in their build directory.
`pytest` is used for testing the installed executables, by calling the exe in a subprocess from python.

### Adding test data

The data for tests is stored at [https://data.3dgi.xyz/roofer-test-data](https://data.3dgi.xyz/roofer-test-data). To add new data, upload a zip of the data files only. The toml configuration is checked into git and placed into `tests/config`. Make sure to use consistent names for the data files and tests.

See `tests/CMakeLists.txt` how to fetch the data from the server and make it available for the tests. Note that `FetchContent` extracts the zip contents recursively. Thus, specify the directory as for instance `SOURCE_DIR "${DATA_DIR}/wippolder"` to have the contents placed into `data/wippolder`.

## Documentation

Dependencies:
- [Doxygen](https://www.doxygen.nl/index.html)
- [graphviz](https://www.graphviz.org) for the graphs (using `dot`)

The documentation needs to be built separately with the `docs` target.
The rendered documentation is in the `docs/html` directory, and the main page is `docs/html/index.html`.

```shell
cmake --build . --target docs
```

#### Documenting the code

This library uses the Javadoc style to document the code.
That is the comment block starts with two *'s.

Full [Doxygen documentation](https://www.doxygen.nl/manual/docblocks.html#specialblock).
