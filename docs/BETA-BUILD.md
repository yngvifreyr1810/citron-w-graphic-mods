# Beta v0.1 build notes

The Windows x64 beta was built from this tag with MSYS2 CLANG64 (Clang 23.1.4), Ninja, CMake 4.4.4 and the Qt 6.9.3 llvm-mingw_64 package. It uses Release optimisation without LTO or PGO. The target remains named `citron` for compatibility with the upstream build scripts; release packaging renames the executable.

See [Windows build instructions](BUILDING-CITRON-WINDOWS.md) to install the toolchain. CPM resolves dependencies from the revisions in `CMakeModules/dependencies.cmake`. The bundled Sirit is used because the older remote revision lacks the quad-swap emitter needed by this shader recompiler.

Important configuration for this beta:

```sh
cmake -S . -B build/beta -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCITRON_BUILD_TYPE=Stable \
  -DCITRON_CHECK_SUBMODULES=OFF -DCITRON_USE_CPM=ON \
  -DCITRON_ENABLE_LTO=OFF -DCITRON_ENABLE_PGO_USE=OFF \
  -DCITRON_USE_BUNDLED_QT=OFF -DCMAKE_PREFIX_PATH=/path/to/Qt/6.9.3/llvm-mingw_64 \
  -DCITRON_USE_PRECOMPILED_HEADERS=OFF -DENABLE_QT_TRANSLATION=OFF \
  -DCITRON_USE_QT_WEB_ENGINE=OFF -DCITRON_USE_QT_MULTIMEDIA=OFF
cmake --build build/beta --target citron --parallel 6
```

Use the CLANG64 environment and its compiler/runtime consistently. The dependency cache and generated build files are intentionally not committed. This describes the build configuration, not a claim of byte-for-byte reproducibility across machines.

Focused tests:

```sh
cmake -S tools/graphic_packs_tests -B build/pack-checks -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.9.3/llvm-mingw_64
cmake --build build/pack-checks
build/pack-checks/graphic_packs_tests -platform offscreen
```

`tools/release/package_windows.py` packages a deployed app folder with an explicit runtime allowlist. It never includes an existing `user` profile, ROMs, keys or saves. Copy dependency licence notices alongside the release before distributing.
