#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/when_any.hpp
 * @brief Await combinator that resolves when the first awaitable completes.
 *
 * `when_any()` is useful for race-style coordination such as timeout vs work,
 * or selecting the first producer to finish.
 */

#include <coroutine>
#include <webcraft/async/async_event.hpp>
#include <webcraft/async/when_all.hpp>
#include <webcraft/async/fire_and_forget_task.hpp>

namespace webcraft::async
{
    namespace detail
    {

        template <awaitable_t Awaitable>
            requires std::same_as<awaitable_resume_t<Awaitable>, void>
        task<void> wait_and_trigger(Awaitable &&awaitable, async_event &evt, std::atomic<bool> &winner)
        {
            co_await awaitable;
            bool expected = false;
            if (winner.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            {
                evt.set(); // Notify the event
            }
            co_return;
        }

        template <std::ranges::input_range Range,
                  typename T = std::ranges::range_value_t<Range>,
                  typename Result = awaitable_resume_t<T>>
            requires awaitable_t<T> && std::same_as<Result, void>
        fire_and_forget_task when_any_controller(Range &&tasks, async_event &event)
        {
            std::atomic<bool> winner = false;
            std::vector<task<void>> wait_tasks;

            for (auto &&task : tasks)
            {
                wait_tasks.emplace_back(wait_and_trigger(std::forward<decltype(task)>(task), event, winner));
            }

            co_await when_all(wait_tasks);
            co_return;
        }

        template <awaitable_t Awaitable, typename Result = awaitable_resume_t<Awaitable>>
            requires(!std::same_as<Result, void>)
        task<void> wait_and_trigger(Awaitable &&awaitable, async_event &evt, std::atomic<bool> &winner, std::optional<Result> &opt)
        {
            auto value = co_await awaitable;
            bool expected = false;
            if (winner.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            {
                opt = std::forward<Result>(value);
                evt.set(); // Notify the event
            }
            co_return;
        }

        template <std::ranges::input_range Range,
                  typename T = std::ranges::range_value_t<Range>,
                  typename Result = awaitable_resume_t<T>>
            requires awaitable_t<T> && (!std::same_as<Result, void>)
        fire_and_forget_task when_any_controller(Range &&tasks, async_event &event, std::optional<Result> &opt)
        {
            std::atomic<bool> winner = false;
            std::vector<task<void>> wait_tasks;

            for (auto &&task : tasks)
            {
                wait_tasks.emplace_back(wait_and_trigger(std::forward<decltype(task)>(task), event, winner, opt));
            }

            co_await when_all(wait_tasks);
            co_return;
        }

        template <awaitable_t... Awaitable>
        fire_and_forget_task when_any_controller_tuple(async_event &event, std::optional<std::variant<normalized_result_t<Awaitable>...>> &opt, Awaitable &&...t)
        {
            std::atomic<bool> winner = false;
            std::vector<task<void>> wait_tasks;

            auto decorator = [&](auto &&t) -> task<void>
            {
                using Result = awaitable_resume_t<decltype(t)>;
                if constexpr (std::is_void_v<Result>)
                {
                    co_await t;
                    bool expected = false;
                    if (winner.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                    {
                        opt = std::monostate{}; // If already won, set to monostate
                        co_return;              // If already won, do nothing
                    }
                }
                else
                {
                    auto value = co_await t;
                    bool expected = false;
                    if (winner.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                    {
                        opt = std::forward<Result>(value);
                        event.set(); // Notify the event
                    }
                }
            };

            co_await when_all(std::make_tuple(decorator(std::forward<Awaitable>(t))...));
            co_return;
        };
    }

    /**
     * @brief Awaits a range and returns when the first awaitable completes.
     */
    template <std::ranges::input_range Range,
              typename T = std::ranges::range_value_t<Range>,
              typename Result = awaitable_resume_t<T>>
        requires awaitable_t<T>
    task<Result> when_any(Range &&tasks)
    {
        async_event event;

        if constexpr (std::is_void_v<Result>)
        {
            detail::when_any_controller(std::forward<Range>(tasks), event);

            co_await event;
            co_return;
        }
        else
        {
            std::optional<Result> result;

            detail::when_any_controller(std::forward<Range>(tasks), event, result);
            co_await event;

            co_return result.value();
        }
    }

    /**
     * @brief Variadic overload that races awaitables and returns the first result as a variant.
     */
    template <awaitable_t... Tasks>
    task<std::variant<normalized_result_t<Tasks>...>> when_any(Tasks &&...tasks)
    {
        using result_variant_t = std::variant<normalized_result_t<Tasks>...>;

        async_event event;
        std::optional<result_variant_t> result;
        detail::when_any_controller_tuple(event, result, std::forward<Tasks>(tasks)...);
        co_await event;
        co_return std::move(result.value());
    }
}