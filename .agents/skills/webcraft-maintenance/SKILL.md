---
name: webcraft-maintenance
description: Maintain WebCraft C++23 runtime, coroutine, native I/O, build, and test code. Use for behavior fixes and library changes in this repository.
---

Read root `AGENTS.md`, `CONTRIBUTING.md`, and the affected headers, native implementations, and tests. Resolve paths from the repository root.

Trace the affected operation through public API, coroutine suspension, native completion, cancellation, and cleanup. Preserve eager task execution and verify ownership of handles, callbacks, descriptors, and buffers across suspension. Shared changes need review against Linux io_uring, Windows IOCP, and macOS kqueue paths.

Use the existing GoogleTest suites for focused regression coverage. Inspect the active CMake cache before reusing `build/`; use a separate directory for a different compiler or generator. Follow contributing commands and report the actual configurations tested. A Linux result does not validate Windows or macOS.

Update the relevant Markdown guide or public header contract with behavior changes. Keep dependency submodules out of incidental changes. For benchmark work, read `benchmarks/README.md` and retain reproducible measurement context.
