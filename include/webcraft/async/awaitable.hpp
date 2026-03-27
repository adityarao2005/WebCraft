#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/awaitable.hpp
 * @brief Awaitable concepts and type traits.
 *
 * Include this header when you need compile-time checks for awaitable types
 * or the resolved resume type (`awaitable_resume_t`) in generic coroutine code.
 */

#include <coroutine>
#include <optional>
#include <concepts>

namespace webcraft::async
{
    /** @brief Detects member `operator co_await`. */
    template <typename T>
    concept has_member_operator_co_await_v = requires(T &&t) {
        { t.operator co_await() };
    };

    /** @brief Valid return types for `await_suspend`. */
    template <typename T>
    concept is_awaitable_suspend_type = std::same_as<T, void> || std::convertible_to<T, bool> || std::convertible_to<T, std::coroutine_handle<>>;

    /** @brief Detects awaiter protocol members (`await_ready/suspend/resume`). */
    template <typename T>
    concept has_awaitable_elements = requires(T &&a, std::coroutine_handle<> h) {
        { a.await_ready() } -> std::convertible_to<bool>;
        { a.await_suspend(h) } -> is_awaitable_suspend_type;
        { a.await_resume() };
    };

    /** @brief Detects non-member `operator co_await`. */
    template <typename T>
    concept has_non_member_operator_co_await_v = requires(T &&t) {
        { operator co_await(t) };
    };

    /** @brief Primary concept indicating a type is awaitable. */
    template <typename T>
    concept awaitable_t = has_member_operator_co_await_v<T> || has_awaitable_elements<T> || has_non_member_operator_co_await_v<T>;

    namespace detail
    {
        template <typename T>
            requires(!awaitable_t<T>)
        struct basic_awaiter
        {
            T &t;

            constexpr basic_awaiter(T &t) noexcept : t(t) {}

            constexpr bool await_ready() const noexcept
            {
                return false;
            }

            constexpr void await_suspend(std::coroutine_handle<> h) const noexcept
            {
            }

            constexpr T await_resume() const
            {
                return std::forward<T>(t);
            }
        };

        template <typename T>
        auto get_awaiter(T &&t)
        {
            if constexpr (has_member_operator_co_await_v<T>)
            {
                return t.operator co_await();
            }
            else if constexpr (has_non_member_operator_co_await_v<T>)
            {
                return operator co_await(std::forward<T>(t));
            }
            else if constexpr (has_awaitable_elements<T>)
            {
                return std::move(t);
            }
            else
            {
                return basic_awaiter(t);
            }
        }
    }

    /** @brief Resolved result type produced by awaiting `T`. */
    template <awaitable_t T>
    using awaitable_resume_t = decltype(detail::get_awaiter(std::declval<T>()).await_resume());
}