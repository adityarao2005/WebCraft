# Async I/O

The `webcraft::async::io` namespace provides stream concepts, file and socket implementations, and readable-stream adaptors. Include the [I/O umbrella header](io.hpp) or individual headers.

## Streams

[core.hpp](core.hpp) defines readable, writable, buffered, and closeable stream concepts. A readable stream's single-value `recv()` returns a task containing an optional value; an empty optional denotes end of stream. Buffered operations work with spans and report a count. A short read or write must be handled using the returned count rather than assuming the whole span was transferred.

Keep buffers and stream owners alive across suspension. Check each concrete stream's close and shutdown behavior; do not assume destroying an object synchronously finishes pending I/O.

## Files

[fs.hpp](fs.hpp) exposes `make_file`, readable/writable stream opening, and stream operations. The native implementation is [async_file_io.cpp](../../../../src/webcraft/async_file_io.cpp). For executable examples covering opening, reading, writing, and cleanup, see [file I/O tests](../../../../tests/src/test_async_io_fs.cpp).

## Sockets

[socket.hpp](socket.hpp) exposes TCP sockets, TCP listeners, UDP sockets, and connection information. TCP provides readable and writable streams, while UDP uses datagram send/receive operations. See [socket tests](../../../../tests/src/test_async_io_socket.cpp) for connection, listener, and datagram usage.

TCP is a byte stream: application code must handle message framing and partial transfers. Keep the runtime alive until socket operations and cleanup complete.

## Adaptors

[adaptors.hpp](adaptors.hpp) provides stream transformations and collectors. Start with [adaptor tests](../../../../tests/src/test_async_io_adaptors.cpp) for the supported pipeline syntax and behavior, and [core tests](../../../../tests/src/test_async_io_core.cpp) for stream contracts.

See [Getting started](../../../../docs/getting-started.md) for runtime setup and [Architecture](../../../../docs/architecture.md) for platform backends. Exact overloads and type requirements live in the headers.
