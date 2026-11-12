# Architecture

WebCraft builds a static `WebCraft` library. Public coroutine abstractions and templates live under `include/webcraft`; native operations are implemented under `src/webcraft`.

| Area | Entry points | Coverage |
| --- | --- | --- |
| Tasks and completion | `async/task.hpp`, `task_completion_source.hpp`, `when_all.hpp`, `when_any.hpp` | `tests/src/test_task*.cpp` |
| Runtime and timers | `async/runtime.hpp`, `src/webcraft/runtime.provider.cpp` | `tests/src/test_runtime.cpp` |
| File streams | `async/io/fs.hpp`, `src/webcraft/async_file_io.cpp` | `tests/src/test_async_io_fs.cpp` |
| TCP and UDP | `async/io/socket.hpp`, native TCP/listener/UDP sources | `tests/src/test_async_io_socket.cpp` |
| Stream concepts and adaptors | `async/io/core.hpp`, `adaptors.hpp` | `tests/src/test_async_io_core.cpp`, `test_async_io_adaptors.cpp` |
| Generators, ranges, thread pool | Corresponding headers in `include/webcraft` | Generator, ranges, and thread pool suites |

Paths in the entry-point column are relative to `include/webcraft` unless prefixed with `src/`.

## Runtime backends

Linux uses io_uring, Windows uses IOCP, and macOS uses kqueue. Platform event wrappers live in `include/webcraft/async/runtime/`; native implementations select their backend with preprocessor guards.

On Linux, producers enqueue operations in a concurrent queue; after
initialization, the runtime thread alone prepares and submits SQEs. Producers
coalesce eventfd notifications around the runtime's decision to wait. Completion
callbacks enqueue follow-up work while the loop is awake, and eventfd rearming
is batched with queued submissions. The application queue is separate from
io_uring's submission queue: enabling SQPOLL would change how the kernel consumes
submitted SQEs, not synchronize concurrent userspace access to liburing's
submission state.

On Windows, successful native file and socket operations finish through their
IOCP packet, including operations that report immediate success. Keep the event,
its `OVERLAPPED` storage, and buffers alive until that packet is consumed. Explicit
non-native events may complete inline when no packet is queued.

`runtime_context` initializes and shuts down the runtime. Runtime state is shared in the provider implementation; do not assume separate contexts create independent event loops. Keep the owning context alive until runtime-backed work and cleanup have completed.

## Ownership and completion

`task<T>` starts eagerly and owns a coroutine handle. Native operations connect completion callbacks to coroutine resumption. Changes to cancellation or cleanup must account for callbacks racing with completion, buffer lifetime across suspension, and descriptor ownership. Read the implementation and its tests before changing these contracts.

The `detail` namespaces contain implementation machinery. Applications should prefer public factories and stream interfaces rather than constructing native event wrappers directly.
