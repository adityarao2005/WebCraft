#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/sync_wait.hpp
 * @brief Blocking bridge from awaitables to synchronous callers.
 *
 * Call `sync_wait(awaitable)` at program boundaries (tests, setup code, or
 * adapters) when you need to wait for coroutine completion on the current
 * thread and obtain its result.
 */

#include "event_signal.hpp"
#include "task.hpp"

namespace webcraft::async
{

    namespace detail
    {
        class sync_wait_operation
        {
        public:
            struct promise_type
            {
                event_signal *signal = nullptr;
                sync_wait_operation get_return_object()
                {
                    return sync_wait_operation{std::coroutine_handle<promise_type>::from_promise(*this)};
                }
                std::suspend_always initial_suspend() noexcept { return {}; }
                struct final_awaiter
                {
                    bool await_ready() noexcept { return false; }
                    void await_suspend(std::coroutine_handle<promise_type> h) noexcept
                    {
                        // The frame is suspended before another thread can
                        // observe completion and destroy it.
                        h.promise().signal->set();
                    }
                    void await_resume() noexcept {}
                };
                final_awaiter final_suspend() noexcept { return {}; }
                void return_void() noexcept {}
                void unhandled_exception() noexcept { std::terminate(); }
            };

            explicit sync_wait_operation(std::coroutine_handle<promise_type> handle) : handle(handle) {}
            sync_wait_operation(const sync_wait_operation &) = delete;
            ~sync_wait_operation() { handle.destroy(); }
            void start(event_signal &signal)
            {
                handle.promise().signal = &signal;
                handle.resume();
            }

        private:
            std::coroutine_handle<promise_type> handle;
        };
    }

    /**
     * @brief Blocks the current thread until an awaitable completes.
     * @tparam T Any awaitable type.
     * @param awaitable Awaitable object to execute and wait on.
     * @return The awaitable result for non-void awaitables.
     * @throws Rethrows exceptions captured from the awaited operation.
     */
    template <awaitable_t T>
    awaitable_resume_t<T> sync_wait(T &&awaitable)
    {
        event_signal signal;
        std::exception_ptr exception;

        if constexpr (std::is_void_v<awaitable_resume_t<T>>)
        {

            auto async_fn = [&] -> detail::sync_wait_operation
            {
                try
                {
                    co_await awaitable;
                }
                catch (...)
                {
                    exception = std::current_exception();
                }
            };

            auto task = async_fn();
            task.start(signal);
            signal.wait();
            if (exception)
            {
                std::rethrow_exception(exception);
            }
        }
        else
        {
            std::optional<awaitable_resume_t<T>> result;

            auto async_fn = [&] -> detail::sync_wait_operation
            {
                try
                {
                    auto value = co_await awaitable;
                    result = std::move(value); // Store the result
                }
                catch (...)
                {
                    exception = std::current_exception();
                }
            };
            auto task = async_fn();
            task.start(signal);
            signal.wait();
            if (exception)
            {
                std::rethrow_exception(exception);
            }
            return std::move(result.value());
        }
    }
}