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

            auto async_fn = [&] -> task<void>
            {
                try
                {
                    co_await awaitable;
                }
                catch (...)
                {
                    exception = std::current_exception();
                }
                signal.set();
            };

            auto task = async_fn();

            signal.wait();
            if (exception)
            {
                std::rethrow_exception(exception);
            }
        }
        else
        {
            std::optional<awaitable_resume_t<T>> result;

            auto async_fn = [&] -> task<void>
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
                signal.set();
            };
            auto task = async_fn();
            signal.wait();
            if (exception)
            {
                std::rethrow_exception(exception);
            }
            return result.value();
        }
    }
}