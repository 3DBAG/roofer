This recipe is vendored from ConanCenterIndex recipes/proj/all/conanfile.py
(https://github.com/conan-io/conan-center-index) to package PROJ 9.8.1,
which is not yet available in ConanCenter. The recipe code is unchanged;
conandata.yml specifies the upstream archive and its SHA-256.

Before running conan install from the repository root, run:

```sh
conan export conan/recipes/proj --version=9.8.1
```

CI performs this export automatically. Once ConanCenter provides PROJ 9.8.1,
the local recipe and export steps can be removed.
