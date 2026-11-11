# Contributing to WebCraft

Start with [Getting started](docs/getting-started.md) to initialize submodules and dependencies. Keep changes focused and describe the behavior they change.

## Configure, build, and test

Use Python 3.9+ and CMake 3.21+ after initializing and bootstrapping vcpkg as
shown in [Getting started](docs/getting-started.md). The same entry point selects
`linux-build`, `macos-build`, or `windows-build` for the host:

```sh
python scripts/webcraft.py check --config Debug
```

`check` runs configure, build, and test in order and stops at the first failure.
Each stage can also run separately:

```sh
python scripts/webcraft.py configure --build-dir build --config Debug
python scripts/webcraft.py build --build-dir build --config Debug
python scripts/webcraft.py test --build-dir build --config Debug --filter RuntimeTestSuite
```

Use the same directory and configuration throughout, especially with Visual
Studio. `--jobs` controls build parallelism (default 2). `--define NAME=VALUE`
passes a CMake cache setting. `--cxx-compiler` selects a compiler at configuration;
use a separate `--build-dir` when changing compiler, generator, or sanitizer.
The runner rejects changes to those settings in an existing cache.

On Linux/macOS with GCC or Clang, supported sanitizer builds are available:

```sh
python scripts/webcraft.py check --sanitizer address-undefined --build-dir build-sanitizers --config RelWithDebInfo
```

`address` and `undefined` can also run individually. CMake checks that the
compiler can compile and link the requested instrumentation and applies it to
both the library and test code. CI uses `RelWithDebInfo` so stack traces retain
symbols while coroutine symmetric transfers can be optimized. GCC 13 AddressSanitizer
instrumentation can overflow the stack in the existing generator throughput test
in both Debug and RelWithDebInfo;
retain that failure when comparing configurations rather than reducing its work.
These checks require the sanitizer runtimes;
Windows sanitizer builds through this entry point are unsupported. Sanitizers
do not establish race freedom; no ThreadSanitizer support is claimed.

The runner writes `webcraft.log`, CTest's `Testing/Temporary/` logs, and
`test-results.xml` (JUnit) into the build directory. Every test logs its name,
start/end, elapsed time, and thread identifier; `TEST_TRACE` adds source-located
operation phases. CI uploads logs/results for every platform, including failures,
and runs a separate Linux Clang AddressSanitizer/UndefinedBehaviorSanitizer build.
Run tooling unit tests with `python -m unittest discover -s scripts/tests -v`.
Raw CMake/CTest commands remain available through [Getting started](docs/getting-started.md).

Tests use GoogleTest. Follow the nearby `TEST_SUITE_NAME` and `TEST_CASE` conventions and put native backend coverage in the appropriate platform directory. Tests have a default 10-second timeout configured in `tests/CMakeLists.txt`.

For behavior changes, add regression coverage that observes results, error handling, or resource lifetimes. Complete pending operations before destroying their buffers, coroutine owners, or runtime context. Check cancellation and cleanup when touching native I/O.

## Request CI on a pull request

Post a new comment containing only `!build` in the conversation of an open PR targeting `main`. The repository owner and collaborators with write, maintain, or admin access can request a build; other comments do not start builds.

The workflow resolves the PR's current head commit and runs the platform matrix against that exact revision. The PR receives a `WebCraft / requested build` status with a link to the workflow run. Post another `!build` comment to check a later commit or retry a run.

GitHub uses the workflow on the default branch for `issue_comment` events, so this workflow change must reach `main` before the new command behavior takes effect. The existing automatic runs on pushes to `main`, review requests, and marking a PR ready for review remain enabled; ordinary PR commits do not automatically trigger builds. See the [workflow](.github/workflows/build.yml) and [GitHub's event documentation](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#issue_comment).

CI uses `windows-2022` to match the Windows preset's Visual Studio 2022 generator. Tests run with the configuration that was built.

The macOS job uses Debug builds with symbols for both the static library and test executable. If tests fail, it reruns `AsyncFSTestSuite.TestFileReadAllUsingAdaptors` in isolation. If that rerun lasts five seconds, macOS `sample` captures all thread stacks; the diagnostic rerun is stopped after a 30-second budget plus bounded cleanup. A passing rerun does not clear the original CI failure.

Download the `macos-file-io-diagnostics-*` artifact from the failed run for CTest logs, rerun output, the stack sample (if captured), and the test executable with its `.dSYM` symbols. Artifacts are retained for seven days. On a Mac, rerun the capture manually with:

```sh
python3 .github/scripts/diagnose_macos_test.py build/tests/webcraft_tests build/diagnostics
```

## Known intermittent failure

GCC can intermittently time out during UDP-server shutdown in the socket tests.
Report the exact test, compiler/configuration, and last logged phase separately
from adaptor/timer regressions. The runner and CI preserve the failure status;
they do not retry it, exclude it, or increase the existing 10-second timeout.

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
