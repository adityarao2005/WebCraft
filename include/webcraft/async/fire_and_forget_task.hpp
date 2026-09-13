#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/fire_and_forget_task.hpp
 * @brief Coroutine return type for detached, non-owning asynchronous work.
 *
 * Use `fire_and_forget()` to run a `task<void>` without awaiting its result,
 * typically for orchestration paths where completion is observed elsewhere.
 */

#include <coroutine>

namespace webcraft::async
{

    /**
     * @brief Coroutine return type for detached tasks that are not awaited.
     */
    class fire_and_forget_task
    {
    public:
        /**
         * @brief Promise type for `fire_and_forget_task` coroutines.
         */
        class promise_type
        {
        public:
            /** @brief Produces the fire-and-forget return object. */
            fire_and_forget_task get_return_object()
            {
                return {};
            }

            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }

            void return_void() noexcept {}
            void unhandled_exception() noexcept {}
        };
    };

    /**
     * @brief Starts a `task<void>` in detached fire-and-forget mode.
     */
    inline fire_and_forget_task fire_and_forget(task<void> t)
    {
        co_await t;
        co_return;
    }
}