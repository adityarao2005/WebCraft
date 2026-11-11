# Async runtime

WebCraft uses C++23 coroutines through `webcraft::async`. Include individual headers or the [async umbrella header](async.hpp).

## Tasks and scheduling

[`task<T>`](task.hpp) is a move-only, eager coroutine task: execution begins when the coroutine is called. Await it to obtain the result and propagate stored exceptions. Keep the task owner and any referenced data alive for the operation's lifetime.

[`runtime_context`](runtime.hpp) owns runtime initialization and shutdown. Create it before runtime-backed work and keep it alive until that work finishes. `yield()` schedules a continuation through the runtime; `sleep_for()` suspends for a duration and accepts a stop token. See the [first coroutine example](../../../docs/getting-started.md) and [runtime tests](../../../tests/src/test_runtime.cpp).

[`sync_wait`](sync_wait.hpp) blocks the calling thread until an awaitable completes. Use it at a synchronous application boundary; avoid blocking the runtime thread on work that needs that same thread to complete.

## Composition and completion

- [`when_all`](when_all.hpp) and [`when_any`](when_any.hpp) compose awaitables; inspect the relevant overload for its result type and ownership behavior.
- [`task_completion_source<T>`](task_completion_source.hpp) bridges externally completed operations to tasks.
- [`async_event`](async_event.hpp) provides coroutine notification; [`event_signal`](event_signal.hpp) provides blocking synchronization.
- [`fire_and_forget_task`](fire_and_forget_task.hpp) supports detached work. Await or otherwise coordinate completion when application state or runtime lifetime depends on it.

## Other utilities

[`generator`](generator.hpp) and [`async_generator`](async_generator.hpp) produce sequences. [`thread_pool`](thread_pool.hpp) runs submitted callables on worker threads. See their headers and the corresponding [test suites](../../../tests/src/) for current behavior.

Continue with [async I/O](io/README.md), [architecture](../../../docs/architecture.md), or the [documentation index](../../../docs/README.md).
