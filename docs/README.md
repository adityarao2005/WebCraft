# WebCraft documentation

WebCraft provides C++23 coroutine tasks, a native asynchronous runtime, and file and socket streams.
These Markdown guides are maintained in the repository and can be read without a hosted site.

| Guide | Purpose |
| --- | --- |
| [Getting started](getting-started.md) | Build the library and run a first coroutine |
| [Async runtime](../include/webcraft/async/README.md) | Tasks, scheduling, completion, and lifetimes |
| [Async I/O](../include/webcraft/async/io/README.md) | Streams, files, sockets, and adaptors |
| [Architecture](architecture.md) | Source layout and native backends |
| [Contributing](../CONTRIBUTING.md) | Development setup, tests, and maintenance |
| [Documentation maintenance](documentation.md) | Edit guides and generate API reference |
| [Benchmarks](../benchmarks/README.md) | Reproduce benchmark measurements |

The public headers are the source of truth for API signatures. Follow the guide links to headers and tests for current contracts and executable usage examples.
