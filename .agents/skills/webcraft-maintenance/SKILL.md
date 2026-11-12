---
name: webcraft-maintenance
description: Maintain WebCraft C++23 runtime, coroutine, native I/O, build, and test code. Use for behavior fixes and library changes in this repository.
---

Read root `AGENTS.md`, `CONTRIBUTING.md`, and the affected headers, native implementations, and tests. Resolve paths from the repository root.

Trace the affected operation through public API, coroutine suspension, native completion, cancellation, and cleanup. Preserve eager task execution and verify ownership of handles, callbacks, descriptors, and buffers across suspension. Shared changes need review against Linux io_uring, Windows IOCP, and macOS kqueue paths.

Use the existing GoogleTest suites for focused regression coverage. Inspect the active CMake cache before reusing `build/`; use a separate directory for a different compiler or generator. Follow contributing commands and report the actual configurations tested. A Linux result does not validate Windows or macOS.

Use `python scripts/webcraft.py` as the default entry point for configuring, building, and testing. Use its `configure`, `build`, and `test` stages for focused work, or `check` for the complete sequence. Keep `--build-dir`, `--config`, and `--sanitizer` consistent across stages; use `--filter` for focused CTest coverage and `--define NAME=VALUE` for supported CMake cache overrides. This preserves the runner's configuration checks and build/test logs and results.

Before falling back to raw CMake/CTest or direct compilation, check whether the runner can express the needed configuration. A dependency mismatch alone is not a reason to bypass it. If a runner limitation or a standalone diagnostic requires a fallback, explain the specific reason and report those results separately from validation through the runner. Do not change the compiler, generator, sanitizer, or test workload merely to obtain a passing result.

Update the relevant Markdown guide or public header contract with behavior changes. Keep dependency submodules out of incidental changes. For benchmark work, read `benchmarks/README.md` and retain reproducible measurement context.
