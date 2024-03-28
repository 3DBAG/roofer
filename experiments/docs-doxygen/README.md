# Library documentation with Doxygen

[Doxygen](https://www.doxygen.nl/index.html)

```shell
mkdir build && cd build
```

Build the library:

```shell
cmake .. && cmake --build .
```

The documentation needs to be built separately with the `docs` target. 
The rendered documentation is in the `docs/html` directory, and the main page is `docs/html/index.html`.

```shell
cmake .. && cmake --build . --target docs
```

## Documenting the code

This library uses the Javadoc style to document the code.
That is the comment block starts with two *'s.

Full [Doxygen documentation](https://www.doxygen.nl/manual/docblocks.html#specialblock).