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
        std::atomic<bool> flag{false};

    public:
        /** @brief Returns true when the event has already been set. */
        bool await_ready() { return is_set(); }

        /** @brief Registers the awaiting coroutine for resumption. */
        constexpr void await_suspend(std::coroutine_handle<> h)
        {
            this->handles.push_back(h);
        }

        /** @brief Completes the await operation. */
        constexpr void await_resume() {}

        /**
         * @brief Sets the event and resumes all waiting coroutines.
         */
        void set()
        {
            bool expected = false;
            if (flag.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            {
                // resumes the coroutine
                for (auto &h : this->handles)
                {
                    if (!h.done())
                    {
                        h.resume();
                    }
                }
                this->handles.clear();
            }
        }

        /** @brief Checks whether the event has already been set. */
        bool is_set() const
        {
            return flag.load(std::memory_order_acquire);
        }
    };
}