# Maintaining documentation

## Sources of truth

Write tutorials, setup instructions, architecture notes, and maintenance guides as repository Markdown. Keep exact API contracts beside declarations in public headers. Link to code and tests instead of maintaining large copies of declarations or implementation pseudocode.

Start navigation at [docs/README.md](README.md). Use relative links so guides remain usable in a checkout and on GitHub. Include prerequisites, executable commands, and any relevant ownership or lifetime requirements in examples. Distinguish tested behavior from design goals and benchmark measurements.

## API documentation

Keep API contracts in public header comments. Link guides to the corresponding headers and tests for exact signatures and executable examples. Markdown is the maintained documentation format; no documentation generator is required.

## Hosting

The Markdown guides do not depend on Vercel or any other web host. Hosting retirement is a separate deployment/account action; no hosting removal is implied by a documentation edit.

## Review

Check relative links and filename casing, verify commands against CMake and presets, and compile substantial new C++ examples when feasible. Run `git diff --check`. Documentation-only changes do not require rerunning the entire runtime suite.
