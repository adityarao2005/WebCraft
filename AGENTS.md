# Working on WebCraft

WebCraft is a C++23 coroutine runtime and static library, not a web frontend.
Read [the documentation index](docs/README.md) and the relevant headers/tests before changing behavior.

## Repository map

- `include/webcraft/`: public headers and template implementations.
- `src/webcraft/`: native runtime, file, TCP, and UDP implementations.
- `tests/`: GoogleTest suites; platform tests live under `tests/src/linux`, `windows`, and `mac`.
- `benchmarks/`: standalone benchmark servers and reproduction instructions.
- `docs/*.md`: maintained Markdown guides.
- `vcpkg/`: dependency submodule; avoid incidental edits or pointer updates.

## Maintenance rules

- Match nearby C++ formatting, namespaces, and naming. Keep public interfaces in headers and native implementation details in `src` or `detail` namespaces.
- Preserve eager `task<T>` execution. Review coroutine frame, callback, buffer, descriptor, cancellation, and shutdown lifetimes together when changing async behavior.
- Check Linux io_uring, Windows IOCP, and macOS kqueue paths for shared API changes. Do not claim cross-platform validation from a Linux run.
- Add focused regression coverage for behavior changes using the existing GoogleTest conventions. Do not rewrite unrelated code or benchmark claims.
- Update Markdown guides when public behavior or setup changes; document exact signatures in header comments rather than copying declarations into guides.
- Keep documentation readable on GitHub and offline. Hosting is optional; changing or deleting a deployment requires an explicit request.
- Leave build outputs, dependency trees, and local credentials out of ordinary changes. Never read or expose `.aws` contents for project maintenance.

## Validation

Follow [CONTRIBUTING.md](CONTRIBUTING.md) for configure/build/test commands. Use the existing build configuration when suitable; do not silently switch its compiler or generator. For code changes, build and run focused tests, then the full suite when relevant. Report commands, failures, and untested platforms. For documentation-only changes, check links, examples, and `git diff --check`; a full runtime test run is unnecessary.

## Project skills

Discoverable skills live in `.agents/skills/`:

- `webcraft-maintenance`: runtime, I/O, testing, and build maintenance.
- `webcraft-docs`: Markdown guides and public API comments.
