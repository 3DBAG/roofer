# Unit testing with Catch2, tests run by CTest

[Catch2](https://github.com/catchorg/Catch2)

```shell
make prepare
```

```shell
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE="Debug" -DENABLE_TESTING=ON \&&
cmake --build build --config Debug
```

Run all tests:

```shell
cd build
ctest
```

Why CTest?
Each library has its own unit test executable. CTest runs each of them with a single command. Without CTest we would need to run each unit test exe separately.

[CTest help](https://cmake.org/cmake/help/book/mastering-cmake/chapter/Testing%20With%20CMake%20and%20CTest.html#testing-using-ctest)
