#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/event_signal.hpp
 * @brief Lightweight thread-based signaling primitive for sync waiting.
 *
 * `event_signal` is used by helper utilities such as `sync_wait()` to bridge
 * coroutine completion to blocking caller code.
 */

#include <atomic>
#include <chrono>
#include <thread>

namespace webcraft::async
{

    /**
     * @brief Utility base class that disables copying and moving.
     */
    class immovable
    {
    public:
        immovable() = default;
        immovable(const immovable &) = delete;
        immovable &operator=(const immovable &) = delete;
        immovable(immovable &&) = delete;
        immovable &operator=(immovable &&) = delete;

        ~immovable() = default;
    };

    /**
     * @brief Busy-wait signal for synchronously waiting on async completion.
     */
    class event_signal : public immovable
    {
    private:
        std::atomic<bool> flag;

    public:
        /** @brief Constructs an unset signal. */
        event_signal() : flag(false) {}

        /** @brief Sets the signal state to true. */
        void set() noexcept
        {
            flag.store(true, std::memory_order_release);
        }

        /** @brief Resets the signal state to false. */
        void reset() noexcept
        {
            flag.store(false, std::memory_order_release);
        }

        /** @brief Returns true when the signal is set. */
        bool is_set() const noexcept
        {
            return flag.load(std::memory_order_acquire);
        }

        /**
         * @brief Waits up to a timeout for the signal to be set.
         * @return `true` if the signal was set before timeout, otherwise `false`.
         */
        bool wait_for(std::chrono::milliseconds timeout) const
        {
            auto start = std::chrono::steady_clock::now();
            while (!is_set())
            {
                if (std::chrono::steady_clock::now() - start >= timeout)
                {
                    return false; // Timeout
                }
                std::this_thread::yield(); // Yield to avoid busy waiting
            }
            return true; // Signal was set
        }

        /**
         * @brief Blocks until the signal is set.
         * @return Always returns `true` once signaled.
         */
        bool wait() const
        {
            while (!is_set())
            {
                std::this_thread::yield(); // Yield to avoid busy waiting
            }
            return true; // Signal was set
        }

        /** @brief Function-call shorthand for `is_set()`. */
        bool operator()() const
        {
            return is_set();
        }

        /** @brief Bool conversion shorthand for `is_set()`. */
        explicit operator bool() const
        {
            return is_set();
        }
    };
}