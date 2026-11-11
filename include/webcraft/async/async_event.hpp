#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/async_event.hpp
 * @brief Awaitable event that resumes multiple suspended coroutines.
 *
 * Use `webcraft::async::async_event` as a one-shot async synchronization
 * point. Coroutines can `co_await` the event and are resumed when `set()` is
 * called.
 */

#include <atomic>
#include <mutex>
#include <coroutine>
#include <vector>

namespace webcraft::async
{
    /**
     * @brief One-shot awaitable event for coroutine coordination.
     *
     * Coroutines can suspend on this object and are resumed when `set()` is
     * called. Once set, subsequent awaits complete immediately.
     */
    struct async_event
    {
    private:
        std::vector<std::coroutine_handle<>> handles;
        std::mutex mutex;
        std::atomic<bool> flag{false};

    public:
        /** @brief Returns true when the event has already been set. */
        bool await_ready()
        {
            std::scoped_lock lock(mutex);
            return flag.load(std::memory_order_acquire);
        }

        /** @brief Registers the awaiting coroutine for resumption. */
        bool await_suspend(std::coroutine_handle<> h)
        {
            std::scoped_lock lock(mutex);
            if (flag.load(std::memory_order_acquire))
                return false;
            handles.push_back(h);
            return true;
        }

        /** @brief Completes the await operation. */
        constexpr void await_resume() {}

        /**
         * @brief Sets the event and resumes all waiting coroutines.
         */
        void set()
        {
            std::vector<std::coroutine_handle<>> waiters;
            {
                std::scoped_lock lock(mutex);
                if (flag.exchange(true, std::memory_order_acq_rel))
                    return;
                waiters.swap(handles);
            }
            // Resuming a waiter may destroy this event. Own the list locally and
            // never access the event again once resumption begins.
            for (auto h : waiters)
                h.resume();
        }

        /** @brief Checks whether the event has already been set. */
        bool is_set() const
        {
            return flag.load(std::memory_order_acquire);
        }
    };
}