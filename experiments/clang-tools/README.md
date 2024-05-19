# Formatting and static analysis with Clang Tools, including pre-commit hook

clang-tidy is part of the Extra Clang Tools, clang-format is part of the Core Clang Tools, see [link](https://clang.llvm.org/docs/ClangTools.html).

Requires Python>=3.8 (only standard packages).

## [Clang-Format](https://clang.llvm.org/docs/ClangFormat.html): C++ code formatting

Enable with `cmake -DENABLE_CLANG_FORMAT:BOOL=ON`.

The formatting rules are configured with the `.clang-format` file.

The `cmake/Tools::add_clang_format_target` CMake function generates the `run_clang_format` target. This build target formats the source code files when it is built.

For example:

```shell
cmake --build build --target run_clang_format
```

The helper script `tools/run-clang-format.py` runs clang-format in a pre-commit hook on GitHub Actions.

Your code editor might also have a clang-format integration and able to format the code based on the `.clang-format` config file.

## [Clang-Tidy](https://clang.llvm.org/extra/clang-tidy/): Static linter for C++

Enable with `cmake -DENABLE_CLANG_TIDY:BOOL=ON`.

The linter checks are configured with the `.clang-tidy` file.

The `cmake/Tools::add_clang_tidy_to_target` CMake function generates an additional `<target>_clangtidy` target for each CMake target. These `<target>_clangtidy` targets run clang-tidy on `<target>`.

If you are using Windows for development, see [how to set up Clang Tools](https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html#setup-clang-tooling-using-cmake-on-windows).

The helper script [run-clang-tidy.py](https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py) runs clang-tidy on all/selected files in the directory tree. Located in `tools/run-clang-tidy.py`.

To detect clang-tidy regressions in the changed lines of code, [clang-tidy-diff.py](https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clang-tidy/tool/clang-tidy-diff.py) could be used. Not added to this repo.

For example, run clang-tidy on the library target of this project:

```shell
cmake --build build --target RooferLib_clangtidy
```

Your code editor might also have a clang-tidy integration and able to lint the code based on the `.clang-tidy` config file.

## Pre-commit hook on GitHub

The `pre-commit` GitHub Action and `.pre-commit-config.yaml` configuration runs checks the changed lines with clang-format and reports if there is anything problematic with the formatting.
