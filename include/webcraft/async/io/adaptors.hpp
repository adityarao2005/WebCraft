#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/io/adaptors.hpp
 * @brief Composable stream transformation adaptors for async I/O pipelines.
 *
 * Use adaptor objects in pipe expressions to transform, filter, or reshape
 * async stream payloads while preserving coroutine-friendly flow.
 */

#include <concepts>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <functional>
#include "core.hpp"

namespace webcraft::async::io::adaptors
{

    /**
     * @brief Base helper for pipeable readable-stream adaptors.
     */
    template <typename Derived, typename T>
    struct async_readable_stream_adaptor
    {

        friend auto operator|(async_readable_stream<T> auto &&stream, Derived &adaptor)
        {
            return std::invoke(adaptor, std::move(stream));
        }

        friend auto operator|(async_readable_stream<T> auto &&stream, Derived &&adaptor)
        {
            return std::invoke(std::move(adaptor), std::forward<decltype(stream)>(stream));
        }
    };

    namespace detail
    {
        template <typename InType, typename Func, typename OutType = typename std::invoke_result_t<Func, async_generator<InType>>::value_type>
        class transform_stream_adaptor : public async_readable_stream_adaptor<transform_stream_adaptor<InType, Func, OutType>, InType>
        {
        private:
            Func transform_fn;

        public:
            static_assert(std::is_invocable_r_v<async_generator<OutType>, Func, async_generator<InType>>,
                          "Function must accept an async_generator<InType> and return an async_generator<OutType>");

            explicit transform_stream_adaptor(Func &&fn)
                : transform_fn(std::move(fn)) {}

            async_readable_stream<OutType> auto operator()(async_readable_stream<InType> auto &&stream) const
            {
                auto &&gen = to_async_generator<InType>(std::move(stream));
                auto &&new_gen = transform_fn(std::move(gen));
                return to_readable_stream<OutType>(std::move(new_gen));
            }
        };

    }

    /** @brief Adaptor that transforms stream elements via an async-generator transform function. */
    template <typename InType, typename Func>
    auto transform(Func &&fn)
    {
        static_assert(std::is_invocable_v<Func, async_generator<InType>>,
                      "Function must accept an async_generator<InType> and return an async_generator<OutType>");

        return detail::transform_stream_adaptor<InType, Func>(std::move(fn));
    }

    /** @brief Element-wise mapping adaptor. */
    template <typename InType, typename Func, typename OutType = std::invoke_result_t<Func, InType>>
    auto map(Func &&fn)
    {
        return transform<InType>([fn = std::move(fn)](async_generator<InType> gen) -> async_generator<OutType>
                                 { for_each_async(value, gen,
                                                  {
                                                      co_yield fn(std::move(value));
                                                  }); });
    }

    /** @brief Forwards stream elements to a writable stream while passing values through. */
    template <typename T>
        requires std::is_copy_assignable_v<T>
    auto pipe(async_writable_stream<T> auto &str)
    {
        return transform<T>([&str](async_generator<T> gen) -> async_generator<T>
                            { for_each_async(value, gen,
                                             {
                                                 co_await str.send(value); // Send a copy of value
                                                 co_yield value;
                                             }); });
    }

    /**
     * @brief Collector concept for adaptors that reduce a stream to a value.
     */
    template <typename Derived, typename ToType, typename StreamType>
    concept collector = std::is_invocable_r_v<task<ToType>, Derived, async_generator<StreamType>>;

    namespace detail
    {

        template <typename ToType, typename StreamType, collector<ToType, StreamType> Collector>
        class collector_stream_adaptor : public async_readable_stream_adaptor<collector_stream_adaptor<ToType, StreamType, Collector>, StreamType>
        {
        private:
            Collector collector_fn;

        public:
            explicit collector_stream_adaptor(Collector &&collector_fn) : collector_fn(std::move(collector_fn)) {}

            task<ToType> operator()(async_readable_stream<StreamType> auto &&stream) const
            {
                auto &&gen = to_async_generator<StreamType>(std::move(stream));
                co_return co_await collector_fn(std::move(gen));
            }
        };
    }

    template <typename ToType, typename StreamType, collector<ToType, StreamType> Collector>
    /** @brief Builds a collector adaptor from a collector functor. */
    auto collect(Collector &&collector_func)
    {
        return detail::collector_stream_adaptor<ToType, StreamType, Collector>(std::move(collector_func));
    }

    template <typename T>
    /** @brief Collector adaptor that writes each incoming value to a target stream. */
    auto forward_to(async_writable_stream<T> auto &stream)
    {
        auto collector_func = [&stream](async_generator<T> gen) -> task<void>
        {
            for_each_async(value, gen,
                           {
                               co_await stream.send(std::move(value));
                           });
        };

        return collect<void, T>(std::move(collector_func));
    }

    template <typename T, typename Func>
    /** @brief Retains only values that satisfy a predicate. */
    auto filter(Func &&predicate)
    {
        static_assert(std::is_invocable_r_v<bool, Func, const T &>,
                      "Predicate must accept a const reference to T and return a bool");
        return transform<T>([predicate = std::move(predicate)](async_generator<T> gen) -> async_generator<T>
                            { for_each_async(value, gen,
                                             {
                                                 if (predicate(value))
                                                 {
                                                     co_yield std::move(value);
                                                 }
                                             }); });
    }

    template <typename T>
    /** @brief Retains at most the first `count` values. */
    auto limit(size_t count)
    {
        return transform<T>([count](async_generator<T> gen) -> async_generator<T>
                            {
                                size_t taken = 0;
                                for_each_async(value, gen,
                                               {
                                                   if (taken < count)
                                                   {
                                                       co_yield std::move(value);
                                                       ++taken;
                                                   }
                                               }); });
    }

    template <typename T>
    /** @brief Skips the first `count` values. */
    auto skip(size_t count)
    {
        return transform<T>([count](async_generator<T> gen) -> async_generator<T>
                            {
                                size_t dropped = 0;
                                for_each_async(value, gen,
                                               {
                                                   if (dropped < count)
                                                   {
                                                       ++dropped;
                                                   }
                                                   else
                                                   {
                                                       co_yield std::move(value);
                                                   }
                                               }); });
    }

    template <typename T, typename Func>
    /** @brief Retains values while a predicate remains true. */
    auto take_while(Func &&predicate)
    {
        static_assert(std::is_invocable_r_v<bool, Func, const T &>,
                      "Predicate must accept a const reference to T and return a bool");
        return transform<T>([predicate = std::move(predicate)](async_generator<T> gen) -> async_generator<T>
                            { for_each_async(value, gen,
                                             {
                                                 if (predicate(value))
                                                 {
                                                     co_yield std::move(value);
                                                 }
                                                 else
                                                 {
                                                     co_return;
                                                 }
                                             }); });
    }

    template <typename T, typename Func>
    /** @brief Drops values while a predicate remains true, then yields the rest. */
    auto drop_while(Func &&predicate)
    {
        static_assert(std::is_invocable_r_v<bool, Func, const T &>,
                      "Predicate must accept a const reference to T and return a bool");
        return transform<T>([predicate = std::move(predicate)](async_generator<T> gen) -> async_generator<T>
                            {
                                bool should_drop = true;
                                for_each_async(value, gen,
                                               {
                                                   if (should_drop && predicate(value))
                                                   {
                                                       // Continue dropping values while the predicate is true
                                                   }
                                                   else
                                                   {
                                                       should_drop = false;
                                                       co_yield std::move(value);
                                                   }
                                               }); });
    }

    namespace collectors
    {

        namespace detail
        {
            template <typename T>
            struct reducer_collector
            {
                std::function<T(T, T)> func;

                explicit reducer_collector(std::function<T(T, T)> &&f)
                    : func(std::move(f)) {}

                task<T> operator()(async_generator<T> gen) const
                {
                    auto itr = co_await gen.begin();

                    if (itr == gen.end())
                    {
                        throw std::logic_error("Cannot reduce an empty generator");
                    }
                    T result = *itr;
                    co_await ++itr;

                    while (itr != gen.end())
                    {
                        result = func(std::move(result), *itr);
                        co_await ++itr;
                    }

                    co_return result;
                }
            };

            static_assert(collector<detail::reducer_collector<int>, int, int>,
                          "Reducer function must accept two arguments of type int and return a value of type int");
            template <typename T>
                requires std::is_convertible_v<T, std::string>
            struct joining_collector
            {
            public:
                std::string separator;
                std::string prefix;
                std::string suffix;

                explicit joining_collector(std::string sep = "", std::string pre = "", std::string suf = "")
                    : separator(std::move(sep)), prefix(std::move(pre)), suffix(std::move(suf)) {}

                task<std::string> operator()(async_generator<T> gen) const
                {
                    std::string separator = this->separator;
                    std::ostringstream ss;
                    ss << prefix;
                    ss << co_await detail::reducer_collector<T>([separator](T a, T b)
                                                                { return a + separator + b; })(std::move(gen));
                    ss << suffix;
                    co_return ss.str();
                }
            };

            static_assert(collector<detail::joining_collector<std::string>, std::string, std::string>,
                          "Joining collector must accept a generator of strings and return a string");
            template <typename T>
            struct to_vector_collector
            {
                task<std::vector<T>> operator()(async_generator<T> gen) const
                {
                    std::vector<T> result;
                    for_each_async(value, gen,
                                   {
                                       result.push_back(std::move(value));
                                   });
                    co_return result;
                }
            };

            static_assert(collector<detail::to_vector_collector<int>, std::vector<int>, int>,
                          "To vector collector must accept a generator of ints and return a vector of ints");
            template <typename T, typename KeyType>
            struct group_by_collector
            {
                std::function<KeyType(const T &)> key_fn;

                explicit group_by_collector(std::function<KeyType(const T &)> key_function)
                    : key_fn(std::move(key_function)) {}

                task<std::unordered_map<KeyType, std::vector<T>>> operator()(async_generator<T> gen) const
                {
                    std::unordered_map<KeyType, std::vector<T>> groups;

                    for_each_async(value, gen,
                                   {
                                       KeyType key = key_fn(value);
                                       groups[key].push_back(std::move(value));
                                   });

                    co_return groups;
                }
            };

            static_assert(collector<detail::group_by_collector<int, int>, std::unordered_map<int, std::vector<int>>, int>,
                          "Group by collector must accept a generator of ints and return a map of ints to vectors of ints");
        }

        template <typename T>
        /** @brief Collector factory that reduces stream values with a binary function. */
        auto reduce(std::function<T(T, T)> &&func)
        {
            return detail::reducer_collector<T>(std::move(func));
        }

        template <typename T>
            requires std::is_convertible_v<T, std::string>
        /** @brief Collector factory that concatenates values into a string. */
        auto joining(std::string separator = "", std::string prefix = "", std::string suffix = "")
        {
            return detail::joining_collector<T>{std::move(separator), std::move(prefix), std::move(suffix)};
        }

        template <typename T>
        /** @brief Collector factory that materializes values into `std::vector<T>`. */
        auto to_vector()
        {
            return detail::to_vector_collector<T>{};
        }

        template <typename T, typename KeyType>
        /** @brief Collector factory that groups values by a computed key. */
        auto group_by(std::function<KeyType(const T &)> key_function)
        {
            return detail::group_by_collector<T, KeyType>{std::move(key_function)};
        }
    }

    template <typename T>
        requires std::totally_ordered<T>
    /** @brief Collects the minimum value using total ordering. */
    auto min()
    {
        return collect<T, T>(collectors::reduce<T>([](T a, T b)
                                                   { return std::min(std::move(a), std::move(b)); }));
    }

    template <typename T>
        requires std::totally_ordered<T>
    /** @brief Collects the maximum value using total ordering. */
    auto max()
    {
        return collect<T, T>(collectors::reduce<T>([](T a, T b)
                                                   { return std::max(std::move(a), std::move(b)); }));
    }

    template <typename T>
    /** @brief Constraint for types closed under addition. */
    concept closure_under_addition = requires(T a, T b) {
        { a + b } -> std::convertible_to<T>;
    };

    template <typename T>
        requires closure_under_addition<T>
    /** @brief Collects the additive sum of stream values. */
    auto sum()
    {
        return collect<T, T>(collectors::reduce<T>([](T a, T b)
                                                   { return a + b; }));
    }

    template <typename T>
    /** @brief Finds the first value that matches a predicate. */
    auto find_first(std::function<bool(const T &)> &&predicate)
    {
        return collect<std::optional<T>, T>([predicate = std::move(predicate)](async_generator<T> gen) -> task<std::optional<T>>
                                            {
                                 for_each_async(value, gen,
                                                {
                                                    if (predicate(value))
                                                    {
                                                        co_return std::make_optional(std::move(value));
                                                    }
                                                });
                                 co_return std::nullopt; });
    }

    template <typename T>
    /** @brief Finds the last value that matches a predicate. */
    auto find_last(std::function<bool(const T &)> &&predicate)
    {
        return collect<std::optional<T>, T>([predicate = std::move(predicate)](async_generator<T> gen) -> task<std::optional<T>>
                                            {
                                 std::optional<T> last_match;
                                 for_each_async(value, gen,
                                                {
                                                    if (predicate(value))
                                                    {
                                                        last_match = std::move(value);
                                                    }
                                                });
                                 co_return last_match; });
    }

    template <typename T>
    /** @brief Returns true if any value matches a predicate. */
    auto any_matches(std::function<bool(const T &)> &&predicate)
    {
        return collect<bool, T>([predicate = std::move(predicate)](async_generator<T> gen) -> task<bool>
                                {
                                    for_each_async(value, gen,
                                                   {
                                                       if (predicate(value))
                                                       {
                                                           co_return true;
                                                       }
                                                   });
                                    co_return false; });
    }

    template <typename T>
    /** @brief Returns true if all values match a predicate. */
    auto all_matches(std::function<bool(const T &)> &&predicate)
    {
        return collect<bool, T>([predicate = std::move(predicate)](async_generator<T> gen) -> task<bool>
                                {
                                    for_each_async(value, gen,
                                                   {
                                                       if (!predicate(value))
                                                       {
                                                           co_return false;
                                                       }
                                                   });
                                    co_return true; });
    }

    template <typename T>
    /** @brief Returns true if no values match a predicate. */
    auto none_matches(std::function<bool(const T &)> &&predicate)
    {
        return collect<bool, T>([predicate = std::move(predicate)](async_generator<T> gen) -> task<bool>
                                {
                                    for_each_async(value, gen,
                                                   {
                                                       if (predicate(value))
                                                       {
                                                           co_return false;
                                                       }
                                                   });
                                    co_return true; });
    }
}