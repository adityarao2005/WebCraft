#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/task_completion_source.hpp
 * @brief External completion source for manually-resolved tasks.
 *
 * Use `task_completion_source<T>` when bridging callback-driven code into
 * coroutines and you need to complete a `task<T>` from outside the coroutine.
 */

#include <coroutine>
#include <type_traits>
#include <utility>
#include <exception>
#include <vector>
#include "task.hpp"

namespace webcraft::async
{
    template <typename T>
    class task_completion_source;

    /**
     * @brief Shared state for manual task completion sources.
     */
    class task_completion_source_base
    {
    protected:
        std::coroutine_handle<> handle_;
        std::exception_ptr exception_;

    public:
        task_completion_source_base() noexcept : handle_(), exception_(nullptr) {}
        task_completion_source_base(const task_completion_source_base &) = delete;
        task_completion_source_base &operator=(const task_completion_source_base &) = delete;
        task_completion_source_base(task_completion_source_base &&other) = delete;

        task_completion_source_base &operator=(task_completion_source_base &&other) = delete;

        virtual ~task_completion_source_base() = default;

        /**
         * @brief Completes the source with an exception.
         */
        void set_exception(std::exception_ptr exception)
        {
            if (handle_ && !handle_.done())
            {
                exception_ = std::move(exception);
                handle_.resume();
            }
            else
            {
                throw std::logic_error("Task completion source already completed");
            }
        }

        /**
         * @brief Internal awaiter used by `task_completion_source::task()`.
         */
        struct awaitable
        {
            task_completion_source_base *tcs;

            bool await_ready() const noexcept
            {
                return tcs->handle_ && tcs->handle_.done();
            }

            void await_suspend(std::coroutine_handle<> h) noexcept
            {
                tcs->handle_ = h;
            }

            constexpr void await_resume()
            {
            }
        };
    };

    /**
     * @brief Manual completion source for `task<T>`.
     */
    template <typename T>
    class task_completion_source : public task_completion_source_base
    {
    private:
        std::optional<T> value_;

    public:
        using value_type = T;

        using task_completion_source_base::set_exception;

        task_completion_source() noexcept : task_completion_source_base(), value_(std::nullopt) {}
        task_completion_source(const task_completion_source &) = delete;
        task_completion_source &operator=(const task_completion_source &) = delete;
        task_completion_source(task_completion_source &&other) = delete;
        task_completion_source &operator=(task_completion_source &&other) = delete;

        ~task_completion_source() = default;

        /** @brief Completes the task with a value and resumes any waiter. */
        void set_value(T value)
        {
            if (handle_)
            {
                value_ = std::move(value);
                handle_.resume();
            }
            else
            {
                throw std::logic_error("Task completion source already completed");
            }
        }

        /** @brief Returns the awaitable task linked to this completion source. */
        webcraft::async::task<T> task()
        {
            co_await task_completion_source_base::awaitable{this};
            if (exception_)
            {
                std::rethrow_exception(exception_);
            }
            co_return value_.value();
        }
    };

    /**
     * @brief Manual completion source specialization for `task<void>`.
     */
    template <>
    class task_completion_source<void> : public task_completion_source_base
    {
    public:
        using value_type = void;

        using task_completion_source_base::set_exception;

        task_completion_source() noexcept : task_completion_source_base() {}
        task_completion_source(const task_completion_source &) = delete;
        task_completion_source &operator=(const task_completion_source &) = delete;
        task_completion_source(task_completion_source &&other) = delete;
        task_completion_source &operator=(task_completion_source &&other) = delete;

        ~task_completion_source() = default;

        /** @brief Completes the void task and resumes any waiter. */
        void set_value()
        {
            if (handle_)
            {
                handle_.resume();
            }
            else
            {
                throw std::logic_error("Task completion source already completed");
            }
        }

        /** @brief Returns the awaitable void task linked to this completion source. */
        webcraft::async::task<void> task()
        {
            co_await task_completion_source_base::awaitable{this};
            if (exception_)
            {
                std::rethrow_exception(exception_);
            }
        }
    };
}