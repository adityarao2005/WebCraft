#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/when_all.hpp
 * @brief Await combinator that waits for all awaitables in a range.
 *
 * `when_all()` consumes a range of awaitables and returns results after every
 * operation has completed.
 */

#include <vector>
#include <optional>
#include <ranges>
#include <memory>
#include <atomic>
#include <utility>

#include <webcraft/async/task.hpp>
#include <webcraft/concepts.hpp>
#include <webcraft/ranges.hpp>
#include <variant>

namespace webcraft::async
{
    /**
     * @brief Awaits all awaitables in a range and collects non-void results.
     */
    template <std::ranges::input_range Range,
              typename T = std::ranges::range_value_t<Range>,
              typename Result = awaitable_resume_t<T>>
        requires awaitable_t<T> && (!std::is_void_v<Result>)
    task<std::vector<Result>> when_all(Range &&tasks)
    {
        std::vector<Result> results;
        results.reserve(std::ranges::size(tasks));

        for (auto &&t : tasks)
        {
            results.push_back(co_await t);
        }

        co_return results;
    }

    /**
     * @brief Awaits all void-returning awaitables in a range.
     */
    template <std::ranges::input_range Range,
              typename T = std::ranges::range_value_t<Range>,
              typename Result = awaitable_resume_t<T>>
        requires awaitable_t<T> && std::is_void_v<Result>
    task<void> when_all(Range &&tasks)
    {
        for (auto &&t : tasks)
        {
            co_await t;
        }

        co_return;
    }

    /**
     * @brief Normalizes awaitable result types by mapping `void` to `std::monostate`.
     */
    template <typename T>
    using normalized_result_t = std::conditional_t<
        std::is_void_v<awaitable_resume_t<T>>,
        std::monostate,
        awaitable_resume_t<T>>;

    /**
     * @brief Awaits every awaitable in a tuple and returns a tuple of normalized results.
     */
    template <typename... Tasks>
        requires(awaitable_t<Tasks> && ...)
    task<std::tuple<normalized_result_t<Tasks>...>> when_all(std::tuple<Tasks...> tasks)
    {
        auto await_one = [](auto &&t) -> task<normalized_result_t<decltype(t)>>
        {
            if constexpr (std::same_as<awaitable_resume_t<decltype(t)>, void>)
            {
                co_await t;
                co_return std::monostate{};
            }
            else
            {
                co_return co_await t;
            }
        };

        auto await_many = [&]<std::size_t... Is>(std::index_sequence<Is...>) -> task<std::tuple<normalized_result_t<Tasks>...>>
        {
            co_return std::tuple<normalized_result_t<Tasks>...>{
                co_await await_one(std::get<Is>(tasks))...};
        };

        co_return co_await await_many(std::make_index_sequence<sizeof...(Tasks)>{});
    }

    /**
     * @brief Variadic convenience overload for `when_all`.
     */
    template <typename... Tasks>
        requires(awaitable_t<Tasks> && ...)
    auto when_all(Tasks &&...tasks)
    {
        return when_all(std::make_tuple(std::forward<Tasks>(tasks)...));
    }
}