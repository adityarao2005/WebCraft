---
name: webcraft-docs
description: Write or maintain WebCraft Markdown guides and public API comments, including setup, API usage, architecture, and contribution documentation.
---

Read root `AGENTS.md`, `docs/README.md`, and `docs/documentation.md`. Resolve project paths from the repository root.

Verify claims against the actual public headers, native implementations, CMake configuration, and tests. Old API sketches may be stale. Prefer practical explanations and short examples over copied class declarations; link to headers for signatures and tests for additional usage.

Keep setup instructions aligned with `CMakePresets.json`, `vcpkg.json`, and `tests/CMakeLists.txt`. Explain runtime and buffer lifetimes in async examples. Do not promise compiler compatibility or platform success without evidence.

Use relative Markdown links and update the documentation index when adding guides. Keep API contracts in public header comments and practical explanations in Markdown. Keep hosting optional; editing docs does not authorize removing a deployment.

Check links, filename casing, code examples, and `git diff --check`. Compile meaningful new examples when feasible; report unavailable prerequisites without claiming validation.
