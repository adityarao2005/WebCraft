# Contributing to WebCraft

Start with [Getting started](docs/getting-started.md) to initialize submodules and dependencies. Keep changes focused and describe the behavior they change.

## Configure, build, and test

Linux example, from the repository root:

```sh
cmake --preset linux-build -DWEBCRAFT_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Use `macos-build` or `windows-build` on those platforms. The presets share `build/`, so use `-B build-clang` or another separate directory when changing toolchains. Use the same directory in subsequent commands. Match `-C` to the configuration you built, especially with Visual Studio.

List discovered tests and run a focused suite:

```sh
ctest --test-dir build -C Debug -N
ctest --test-dir build -C Debug --output-on-failure -R RuntimeTestSuite
```

Tests use GoogleTest. Follow the nearby `TEST_SUITE_NAME` and `TEST_CASE` conventions and put native backend coverage in the appropriate platform directory. Tests have a default 10-second timeout configured in `tests/CMakeLists.txt`.

For behavior changes, add regression coverage that observes results, error handling, or resource lifetimes. Complete pending operations before destroying their buffers, coroutine owners, or runtime context. Check cancellation and cleanup when touching native I/O.

## Troubleshooting

- Missing GoogleTest: enable the preset's vcpkg toolchain and initialize/bootstrap the submodule.
- Missing liburing: check the vcpkg configure output and pkg-config discovery on Linux.
- io_uring initialization failure: check whether the kernel and execution environment permit io_uring. A successful build does not establish that runtime operations are allowed.
- Generator/compiler mismatch: configure into a fresh build directory rather than overwriting an unrelated CMake cache.
- C++23 library errors: inspect both the compiler and standard library in use; a compiler version alone does not establish feature availability.

## Review and maintenance

Follow the style of the surrounding code. Keep public API documentation in headers and practical explanations in [Markdown guides](docs/README.md). Update the relevant guide with API or setup changes. Avoid incidental edits to dependency submodules.

Before a PR, run `git diff --check`, the relevant build and tests, and review `git diff --stat` for accidental artifacts. State what passed, what failed, and which platforms were not tested. CI defines Linux GCC/Clang, Windows MSVC, and macOS Clang configurations in [build.yml](.github/workflows/build.yml); inspect the actual run before claiming they passed.

Benchmark results need reproducible commands and environment details; follow [the benchmark guide](benchmarks/README.md).
