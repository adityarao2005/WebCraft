# Async I/O

The `webcraft::async::io` namespace provides stream concepts, file and socket implementations, and readable-stream adaptors. Include the [I/O umbrella header](io.hpp) or individual headers.

## Streams

[core.hpp](core.hpp) defines readable, writable, buffered, and closeable stream concepts. A readable stream's single-value `recv()` returns a task containing an optional value; an empty optional denotes end of stream. Buffered operations work with spans and report a count. A short read or write must be handled using the returned count rather than assuming the whole span was transferred.

Keep buffers and stream owners alive across suspension. Check each concrete stream's close and shutdown behavior; do not assume destroying an object synchronously finishes pending I/O.

## Files

[fs.hpp](fs.hpp) exposes `make_file`, readable/writable stream opening, and stream operations. The native implementation is [async_file_io.cpp](../../../../src/webcraft/async_file_io.cpp). For executable examples covering opening, reading, writing, and cleanup, see [file I/O tests](../../../../tests/src/test_async_io_fs.cpp).

Failed file opens propagate `std::system_error` when the opening task is awaited.
Readable opens do not create missing files. Writable opens can create a file, but
its parent directory must already exist.

## Sockets

[socket.hpp](socket.hpp) exposes TCP sockets, TCP listeners, UDP sockets, and connection information. TCP provides readable and writable streams, while UDP uses datagram send/receive operations. See [socket tests](../../../../tests/src/test_async_io_socket.cpp) for connection, listener, and datagram usage.

Windows and macOS run UDP sends and receive attempts on a worker pool by default. A pending
receive completes with zero when it observes that the socket has closed. Keep
the task, buffer, and sender endpoint alive until the receive completes. Finish
pending operations before rebinding the socket.

### UDP multicast

To receive multicast datagrams, bind a UDP socket to the wildcard address for
the group's IP family (`0.0.0.0` or `::`) and the destination port, then join
the group. The membership calls take a numeric group address and an optional
local interface. Use a local IPv4 address for IPv4 or an interface index for
IPv6. An empty interface selects the system default, if one is available.

```cpp
// Inside a task with an active runtime_context.
using namespace webcraft::async::io::socket;
const connection_info group{"239.255.42.78", 5000};
auto receiver = make_udp_socket(ip_version::IPv4);
receiver.bind({"0.0.0.0", group.port});
receiver.join_multicast_group(group.host);

std::array<char, 1500> buffer{};
connection_info sender;
auto received = co_await receiver.recvfrom(buffer, sender);

receiver.leave_multicast_group(group.host);
co_await receiver.close();
```

Send to the group address and port with `sendto`. Sending uses the system route
and does not require joining the group. For scoped IPv6 destinations, include
the scope in the endpoint host as required by the system. Membership is tied
to the bound socket and ends when that socket closes or is rebound. Use the
same interface when leaving a group and serialize membership changes with
bind and close. Keep the datagram buffer and sender endpoint alive until
`recvfrom` completes.

TCP is a byte stream: application code must handle message framing and partial transfers. Keep the runtime alive until socket operations and cleanup complete.

## Adaptors

[adaptors.hpp](adaptors.hpp) provides stream transformations and collectors. Start with [adaptor tests](../../../../tests/src/test_async_io_adaptors.cpp) for the supported pipeline syntax and behavior, and [core tests](../../../../tests/src/test_async_io_core.cpp) for stream contracts.

Adaptor element types are inferred from the stream on the left of `|`, including after a mapping changes the payload type. Generic lambdas work too:

```cpp
using namespace webcraft::async::io::adaptors;
auto values = co_await (source
    | map([](int value) { return std::to_string(value); })
    | filter([](const std::string &value) { return value.size() > 1; })
    | collect(collectors::to_vector()));
```

The same inference applies to `transform`, `skip`, `limit`, predicate adaptors, sinks, and collectors. Existing explicitly typed calls remain supported. Element-type inference happens at compile time.

Applying an adaptor moves its input stream into the pipeline, including named input streams. The pipeline owns its transformation callbacks and intermediate streams, so it can be saved and consumed later. Writable targets passed to `pipe` and `forward_to` are borrowed and must remain alive until consumption finishes. Copyable inferred adaptors can be reused with different stream element types; move an adaptor with move-only captures into the pipeline.

See [Getting started](../../../../docs/getting-started.md) for runtime setup and [Architecture](../../../../docs/architecture.md) for platform backends. Exact overloads and type requirements live in the headers.


`sorted()` buffers a finite input until EOF, then yields stable ascending order;
pass a comparator for another ordering. The comparator and buffered values remain
owned across suspension. Infinite input cannot produce sorted output.

Zip adaptors combine streams by position (not by key):

| Adaptor | Stops at | Element type |
| --- | --- | --- |
| `zip_inner(right)` | Either EOF | `pair<L, R>` |
| `zip_left(right)` | Left EOF | `pair<L, optional<R>>` |
| `zip_right(right)` | Right EOF | `pair<optional<L>, R>` |
| `zip_full(right)` | Both EOFs | `pair<optional<L>, optional<R>>` |

For example, `left | zip_full(right)` moves both inputs into the resulting
pipeline. Missing values are `nullopt`. Receives are sequential, with the left
input read first except for `zip_right`; inner zip can consume one unmatched left
value when the right input ends. EOF is remembered so ended inputs are not polled
again. Exceptions propagate and terminate the zip. Keep the resulting stream
alive until its eager receive task completes; do not destroy a suspended receive.
Sorting, zipping, and collecting into a vector support move-only elements.
