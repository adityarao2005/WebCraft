#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/task.hpp
 * @brief Core coroutine task type used across WebCraft async APIs.
 *
 * `webcraft::async::task<T>` represents an awaitable operation that produces
 * a value of type `T` (or `void`). Most high-level async APIs in this project
 * return this type.
 */

#include <concepts>
#include <exception>
#include <coroutine>
#include "awaitable.hpp"
#include "event_signal.hpp"
#include <iostream>
#include <ranges>
#include <utility>
#include <functional>

namespace webcraft::async
{
    // Forward declare
    template <typename T>
    class task;

    /**
     * @brief Promise type backing `task<T>` coroutines.
     */
    template <typename T>
    class task_promise
    {
    public:
        std::optional<T> value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        /** @brief Builds the `task<T>` object for this coroutine. */
        task<T> get_return_object();

        std::suspend_never initial_suspend() noexcept { return {}; }

        /** @brief Final awaiter that resumes the continuation coroutine. */
        struct final_awaiter
        {
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<task_promise> h) noexcept
            {
                if (h.promise().continuation)
                    h.promise().continuation.resume();
            }
            void await_resume() noexcept {}
        };
        final_awaiter final_suspend() noexcept { return {}; }

        void return_value(T v) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            value = std::move(v);
        }

        // void return_value(const T &v) noexcept(std::is_nothrow_copy_constructible_v<T>)
        // {
        //     value = v;
        // }

        // void return_value(T &&v) noexcept(std::is_nothrow_move_constructible_v<T>)
        // {
        //     value = std::move(v);
        // }

        void unhandled_exception() noexcept
        {
            exception = std::current_exception();
        }
    };

    /**
     * @brief Promise specialization for `task<void>` coroutines.
     */
    template <>
    class task_promise<void>
    {
    public:
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        /** @brief Builds the `task<void>` object for this coroutine. */
        task<void> get_return_object();

        std::suspend_never initial_suspend() noexcept { return {}; }

        /** @brief Final awaiter that resumes the continuation coroutine. */
        struct final_awaiter
        {
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<task_promise> h) noexcept
            {
                if (h.promise().continuation)
                    h.promise().continuation.resume();
            }
            void await_resume() noexcept {}
        };
        final_awaiter final_suspend() noexcept { return {}; }

        void return_void() noexcept {}

        void unhandled_exception() noexcept
        {
            exception = std::current_exception();
        }
    };

    /**
     * @brief Awaitable coroutine result that yields a value of type `T`.
     */
    template <typename T = void>
    class task
    {
    public:
        using promise_type = task_promise<T>;
        using handle_type = std::coroutine_handle<promise_type>;

        explicit task(handle_type h) noexcept : coro(h) {}

        task(task &&other) noexcept : coro(std::exchange(other.coro, {})) {}
        task &operator=(task &&other) noexcept
        {
            if (this != &other)
            {
                if (coro)
                    coro.destroy();
                coro = std::exchange(other.coro, {});
            }
            return *this;
        }

        ~task()
        {
            if (coro)
                coro.destroy();
        }

        task(const task &) = delete;
        task &operator=(const task &) = delete;

        bool await_ready() const noexcept
        {
            return !coro || coro.done();
        }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            coro.promise().continuation = h;
        }

        T await_resume()
        {
            if (coro.promise().exception)
                std::rethrow_exception(coro.promise().exception);
            return std::move(coro.promise().value.value());
        }

    private:
        friend class task_promise<T>;
        handle_type coro;
    };

    /**
     * @brief Awaitable coroutine result specialization for `void`.
     */
    template <>
    class task<void>
    {
    public:
        using promise_type = task_promise<void>;
        using handle_type = std::coroutine_handle<promise_type>;

        explicit task(handle_type h) noexcept : coro(h) {}

        task(task &&other) noexcept : coro(std::exchange(other.coro, {})) {}
        task &operator=(task &&other) noexcept
        {
            if (this != &other)
            {
                if (coro)
                    coro.destroy();
                coro = std::exchange(other.coro, {});
            }
            return *this;
        }

        ~task()
        {
            if (coro)
                coro.destroy();
        }

        task(const task &) = delete;
        task &operator=(const task &) = delete;

        bool await_ready() const noexcept
        {
            return !coro || coro.done();
        }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            coro.promise().continuation = h;
        }

        void await_resume()
        {
            if (coro.promise().exception)
                std::rethrow_exception(coro.promise().exception);
        }

    private:
        friend class task_promise<void>;
        handle_type coro;
    };

    // Define get_return_object after task is complete
    template <typename T>
    task<T> task_promise<T>::get_return_object()
    {
        return task<T>{std::coroutine_handle<task_promise>::from_promise(*this)};
    }

    inline task<void> task_promise<void>::get_return_object()
    {
        return task<void>{std::coroutine_handle<task_promise>::from_promise(*this)};
    }

    /**
     * @brief Base class for pipe-style task adaptors.
     */
    template <typename Derived>
    struct task_adaptor_closure
    {
        template <typename Awaitable>
        friend auto operator|(Awaitable &&awaitable, Derived &&self)
        {
            return self(std::forward<Awaitable>(awaitable));
        }

        template <typename Awaitable>
        friend auto operator|(Awaitable &&awaitable, Derived &self)
        {
            return self(std::forward<Awaitable>(awaitable));
        }

        template <typename Awaitable>
        friend auto operator|(Awaitable &&awaitable, const Derived &self)
        {
            return self(std::forward<Awaitable>(awaitable));
        }
    };

    namespace detail
    {
        template <typename Func>
        class then_adaptor_closure : public task_adaptor_closure<then_adaptor_closure<Func>>
        {
        private:
            Func func;

        public:
            explicit then_adaptor_closure(Func f) : func(std::move(f)) {}

            template <awaitable_t Awaitable>
            auto operator()(Awaitable &&awaitable) const
            {
                using AwaitedType = awaitable_resume_t<Awaitable>;

                if constexpr (std::is_void_v<AwaitedType>)
                {
                    using Result = std::invoke_result_t<Func>;

                    auto task_fn = [](Awaitable &&awaitable, const Func &func) -> task<Result>
                    {
                        if constexpr (awaitable_t<Result>)
                        {
                            co_await std::forward<Awaitable>(awaitable);
                            co_return co_await func();
                        }
                        else
                        {
                            co_await std::forward<Awaitable>(awaitable);
                            co_return func();
                        }
                    };

                    return task_fn(std::forward<Awaitable>(awaitable), func);
                }
                else
                {
                    using Result = std::invoke_result_t<Func, AwaitedType>;

                    auto task_fn = [](Awaitable &&awaitable, const Func &func) -> task<Result>
                    {
                        if constexpr (awaitable_t<Result>)
                        {
                            co_return co_await func(co_await std::forward<Awaitable>(awaitable));
                        }
                        else
                        {
                            co_return func(co_await std::forward<Awaitable>(awaitable));
                        }
                    };
                    return task_fn(std::forward<Awaitable>(awaitable), func);
                }
            }
        };

        template <typename Func>
        class upon_error_adaptor_closure : public task_adaptor_closure<upon_error_adaptor_closure<Func>>
        {
        private:
            Func handler;

        public:
            upon_error_adaptor_closure(Func &&h)
                : handler(std::move(h)) {}
            static_assert(std::is_invocable_v<Func, std::exception_ptr>,
                          "Handler must be invocable with std::exception_ptr");

            template <awaitable_t Awaitable>
            task<awaitable_resume_t<Awaitable>> operator()(Awaitable &&awaitable) const
            {
                static_assert(std::convertible_to<awaitable_resume_t<Awaitable>, std::invoke_result_t<Func, std::exception_ptr>>,
                              "Awaitable result must be convertible to the handler's result type");

                using ResultType = awaitable_resume_t<Awaitable>;

                try
                {
                    if constexpr (std::is_void_v<ResultType>)
                    {
                        co_await std::forward<Awaitable>(awaitable);
                    }
                    else
                    {
                        co_return co_await std::forward<Awaitable>(awaitable);
                    }
                }
                catch (...)
                {
                    co_return handler(std::current_exception());
                }
            }
        };
    }

    /**
     * @brief Creates a continuation adaptor to transform successful task results.
     */
    template <typename Func>
    auto then(Func &&func)
    {
        return detail::then_adaptor_closure<std::decay_t<Func>>{std::forward<Func>(func)};
    }

    /**
     * @brief Creates an error-recovery adaptor for failed awaitables.
     */
    template <typename Func>
    auto upon_error(Func &&handler)
    {
        return detail::upon_error_adaptor_closure<std::decay_t<Func>>{std::forward<Func>(handler)};
    }

}