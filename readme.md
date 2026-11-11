# WebCraft

> The async runtime for C++ leveraging powerful features of C++23 built for scale, speed, and ease.

NOTE: This codebase is recommended to run on C++ 23 since most of the features are geared to it. If its possible try to get your compiler to compile with experimental libraries enabled (which will enable all the features the compiler may not have implemented for C++ 23).

The following compilers (and anything newer) are supported: GCC 13, MSVC 2022, Clang 17

## Features / Modules

1. Asynchronous Runtime, Supporting Yielding and working with timeouts that are not thread based
2. Basic networking API (TCP & UDP Sockets, async file read & write) via I/O streams.

## Benchmarks

WebCraft's `io_uring`-based async runtime delivers **higher throughput than Nginx** on a single-process "Hello, World!" HTTP benchmark using [`wrk`](https://github.com/wg/wrk):

| Server | Requests/sec | Avg Latency | Transfer/sec |
|---|---:|---:|---:|
| **WebCraft** | **160,096** | 12.62ms | 30.69MB |
| Nginx 1.24 | 120,695 | 0.90ms | 30.62MB |
| Node.js | 39,291 | 4.08ms | 8.88MB |

> Tested with `wrk -t4 -c100 -d10s` on Linux — single worker process for all servers.

To run the benchmarks yourself, see the [benchmarks/](benchmarks/) directory.

## Documentation

Read the [Markdown documentation](docs/README.md) for setup, coroutine usage, async I/O, and architecture. The guides work directly on GitHub and in a local checkout, without a hosted documentation site.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup, platform presets, tests, and maintenance conventions. Agent guidance lives in [AGENTS.md](AGENTS.md), with project skills under `.agents/skills/`.

## Using it in your project

We've got a vcpkg port for it. Run `vcpkg install webcraft` or add it in your vcpkg.json. An example of its usage is here to build a simple HTTP server: https://github.com/adityarao2005/http-server-webcraft
