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

#include <algorithm>
#include <utility>
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
        template <typename Result>
        struct stream_value;

        template <typename T>
        struct stream_value<task<std::optional<T>>>
        {
            using type = T;
        };

        template <typename Stream>
        using stream_value_t = typename stream_value<decltype(std::declval<Stream &>().recv())>::type;

        // Delay element-type selection until the left-hand stream is known.
        template <typename Func>
        struct inferred_stream_adaptor
        {
            Func fn;

            template <typename Stream, typename T = stream_value_t<Stream>>
                requires async_readable_stream<Stream, T>
            friend auto operator|(Stream &&stream, inferred_stream_adaptor adaptor)
            {
                return adaptor.fn.template operator()<T>(std::forward<Stream>(stream));
            }
        };

        template <typename InType, typename Func, typename OutType = typename std::invoke_result_t<Func, async_generator<InType>>::value_type>
        class transform_stream_adaptor : public async_readable_stream_adaptor<transform_stream_adaptor<InType, Func, OutType>, InType>
        {
        private:
            Func transform_fn;

            // Keep both the callback and source alive while lazy generators suspend.
            template <typename Stream>
            static async_generator<OutType> generate(Func fn, Stream stream)
            {
                auto gen = to_async_generator<InType>(std::move(stream));
                auto transformed = fn(std::move(gen));
                auto it = co_await transformed.begin();
                while (it != transformed.end())
                {
                    co_yield std::move(*it);
                    co_await ++it;
                }
            }

        public:
            static_assert(std::is_invocable_r_v<async_generator<OutType>, Func, async_generator<InType>>,
                          "Function must accept an async_generator<InType> and return an async_generator<OutType>");

            explicit transform_stream_adaptor(Func fn)
                : transform_fn(std::move(fn)) {}

            async_readable_stream<OutType> auto operator()(async_readable_stream<InType> auto &&stream) const &
            {
                return to_readable_stream<OutType>(generate(transform_fn, std::move(stream)));
            }

            async_readable_stream<OutType> auto operator()(async_readable_stream<InType> auto &&stream) &&
            {
                return to_readable_stream<OutType>(generate(std::move(transform_fn), std::move(stream)));
            }
        };

    }

    /** @brief Adaptor that transforms stream elements via an async-generator transform function. */
    template <typename InType, typename Func>
    auto transform(Func &&fn)
    {
        static_assert(std::is_invocable_v<Func, async_generator<InType>>,
                      "Function must accept an async_generator<InType> and return an async_generator<OutType>");

        return detail::transform_stream_adaptor<InType, std::decay_t<Func>>(std::forward<Func>(fn));
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

            template <typename Stream>
            static task<ToType> consume(Collector fn, Stream stream)
            {
                auto gen = to_async_generator<StreamType>(std::move(stream));
                co_return co_await fn(std::move(gen));
            }

        public:
            explicit collector_stream_adaptor(Collector collector_fn) : collector_fn(std::move(collector_fn)) {}

            task<ToType> operator()(async_readable_stream<StreamType> auto &&stream) const &
            {
                return consume(collector_fn, std::move(stream));
            }

            task<ToType> operator()(async_readable_stream<StreamType> auto &&stream) &&
            {
                return consume(std::move(collector_fn), std::move(stream));
            }
        };
    }

    template <typename ToType, typename StreamType, collector<ToType, StreamType> Collector>
    /** @brief Builds a collector adaptor from a collector functor. */
    auto collect(Collector &&collector_func)
    {
        return detail::collector_stream_adaptor<ToType, StreamType, std::decay_t<Collector>>(std::forward<Collector>(collector_func));
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
                    auto it = co_await gen.begin();
                    while (it != gen.end())
                    {
                        result.push_back(std::move(*it));
                        co_await ++it;
                    }
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

        /** @brief Collector factories with element types inferred by collect(). */
        template <typename Func>
        auto reduce(Func &&fn)
        {
            return [fn = std::forward<Func>(fn)]<typename T>(async_generator<T> gen)
                -> task<T>
            {
                auto collector = reduce<T>(fn);
                co_return co_await collector(std::move(gen));
            };
        }

        inline auto joining(std::string separator = "", std::string prefix = "", std::string suffix = "")
        {
            return [separator = std::move(separator), prefix = std::move(prefix), suffix = std::move(suffix)]<typename T>(async_generator<T> gen)
                -> task<std::string>
            {
                auto collector = joining<T>(separator, prefix, suffix);
                co_return co_await collector(std::move(gen));
            };
        }

        inline auto to_vector()
        {
            return []<typename T>(async_generator<T> gen)
                -> task<std::vector<T>>
            {
                auto collector = to_vector<T>();
                co_return co_await collector(std::move(gen));
            };
        }

        template <typename Func>
        auto group_by(Func &&fn)
        {
            return [fn = std::forward<Func>(fn)]<typename T>(async_generator<T> gen)
                -> task<std::unordered_map<std::invoke_result_t<Func, const T &>, std::vector<T>>>
            {
                auto collector = group_by<T, std::invoke_result_t<Func, const T &>>(fn);
                co_return co_await collector(std::move(gen));
            };
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

    /**
     * @brief Buffers a finite stream, then yields values in comparator order.
     * Owns the comparator and input across suspension. Equal values retain their
     * input order. No output is produced before EOF; infinite streams never yield.
     */
    template <typename T, typename Compare = std::less<>>
    auto sorted(Compare compare = {})
    {
        return transform<T>([compare = std::move(compare)](async_generator<T> gen) mutable -> async_generator<T>
                            {
                                std::vector<T> values;
                                auto it = co_await gen.begin();
                                while (it != gen.end())
                                {
                                    values.push_back(std::move(*it));
                                    co_await ++it;
                                }
                                std::stable_sort(values.begin(), values.end(), std::ref(compare));
                                for (auto &value : values)
                                {
                                    co_yield std::move(value);
                                }
                            });
    }

    template <typename Compare>
    auto sorted(Compare compare)
    {
        return detail::inferred_stream_adaptor{[compare = std::move(compare)]<typename T>(auto &&stream) mutable
                                               { return sorted<T>(std::move(compare))(std::forward<decltype(stream)>(stream)); }};
    }

    inline auto sorted()
    {
        return sorted(std::less<>{});
    }

    namespace detail
    {
        enum class zip_mode { inner, full, left, right };

        template <zip_mode Mode, typename Left, typename Right>
        class zip_stream
        {
            using L = stream_value_t<Left>;
            using R = stream_value_t<Right>;
            using First = std::conditional_t<Mode == zip_mode::full || Mode == zip_mode::right, std::optional<L>, L>;
            using Second = std::conditional_t<Mode == zip_mode::full || Mode == zip_mode::left, std::optional<R>, R>;
            Left left;
            Right right;
            bool left_done = false;
            bool right_done = false;
            bool done = false;

        public:
            zip_stream(Left left, Right right) : left(std::move(left)), right(std::move(right)) {}

            task<std::optional<std::pair<First, Second>>> recv()
            {
                if (done)
                    co_return std::nullopt;
                try
                {
                    std::optional<L> l;
                    std::optional<R> r;
                    // Read the controlling side first. Never poll an input again after EOF.
                    if constexpr (Mode == zip_mode::right)
                    {
                        r = co_await right.recv();
                        if (r && !left_done)
                            l = co_await left.recv();
                    }
                    else
                    {
                        if (!left_done)
                            l = co_await left.recv();
                        if ((l || Mode == zip_mode::full) && !right_done)
                            r = co_await right.recv();
                    }
                    left_done = !l;
                    right_done = !r;
                    if constexpr (Mode == zip_mode::inner)
                        done = !l || !r;
                    else if constexpr (Mode == zip_mode::left)
                        done = !l;
                    else if constexpr (Mode == zip_mode::right)
                        done = !r;
                    else
                        done = !l && !r;
                    if (done)
                        co_return std::nullopt;

                    if constexpr (Mode == zip_mode::inner)
                        co_return std::pair<First, Second>{std::move(*l), std::move(*r)};
                    else if constexpr (Mode == zip_mode::left)
                        co_return std::pair<First, Second>{std::move(*l), std::move(r)};
                    else if constexpr (Mode == zip_mode::right)
                        co_return std::pair<First, Second>{std::move(l), std::move(*r)};
                    else
                        co_return std::pair<First, Second>{std::move(l), std::move(r)};
                }
                catch (...)
                {
                    done = true;
                    throw;
                }
            }
        };

        template <zip_mode Mode, typename Right>
        auto zip(Right &&right)
        {
            return inferred_stream_adaptor{[right = std::move(right)]<typename T>(auto &&left) mutable
                                           {
                                               using Left = std::decay_t<decltype(left)>;
                                               return zip_stream<Mode, Left, std::decay_t<Right>>(std::move(left), std::move(right));
                                           }};
        }
    }

    /**
     * @brief Positionally pairs inputs until either ends; yields pair<L, R>.
     * Inputs are moved into the pipeline, including lvalues. Receives are
     * sequential, left first; a final unmatched left value may be consumed.
     * Keep the pipeline alive until each eager recv task completes. Exceptions
     * propagate and terminate the zip. No pending receive is abandoned at EOF.
     */
    template <typename Right>
    auto zip_inner(Right &&right) { return detail::zip<detail::zip_mode::inner>(std::forward<Right>(right)); }

    /** @brief Pairs through both EOFs; yields pair<optional<L>, optional<R>>. */
    template <typename Right>
    auto zip_full(Right &&right) { return detail::zip<detail::zip_mode::full>(std::forward<Right>(right)); }

    /** @brief Pairs through left EOF; yields pair<L, optional<R>>, reading left first. */
    template <typename Right>
    auto zip_left(Right &&right) { return detail::zip<detail::zip_mode::left>(std::forward<Right>(right)); }

    /** @brief Pairs through right EOF; yields pair<optional<L>, R>, reading right first. */
    template <typename Right>
    auto zip_right(Right &&right) { return detail::zip<detail::zip_mode::right>(std::forward<Right>(right)); }

    /**
     * @brief Inferred adaptor overloads select their element type from the input
     * stream at pipe application. Callables are owned by value; generic lambdas
     * are supported. Explicitly typed overloads remain available. Pipe application
     * moves the input stream into the resulting pipeline, including lvalue streams.
     * Copyable inferred adaptors can be reused; move-only adaptors must be moved.
     * Writable targets remain borrowed and must outlive pipeline consumption.
     */
    template <typename Func>
    auto transform(Func &&fn)
    {
        return detail::inferred_stream_adaptor{[fn = std::forward<Func>(fn)]<typename T>(auto &&stream) mutable
                                               { return transform<T>(std::move(fn))(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Func>
    auto map(Func &&fn)
    {
        return detail::inferred_stream_adaptor{[fn = std::forward<Func>(fn)]<typename T>(auto &&stream) mutable
                                               { return map<T>(std::move(fn))(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Func>
    auto filter(Func &&predicate)
    {
        return detail::inferred_stream_adaptor{[predicate = std::forward<Func>(predicate)]<typename T>(auto &&stream) mutable
                                               { return filter<T>(std::move(predicate))(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Func>
    auto take_while(Func &&predicate)
    {
        return detail::inferred_stream_adaptor{[predicate = std::forward<Func>(predicate)]<typename T>(auto &&stream) mutable
                                               { return take_while<T>(std::move(predicate))(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Func>
    auto drop_while(Func &&predicate)
    {
        return detail::inferred_stream_adaptor{[predicate = std::forward<Func>(predicate)]<typename T>(auto &&stream) mutable
                                               { return drop_while<T>(std::move(predicate))(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Func>
    auto find_first(Func &&predicate)
    {
        return detail::inferred_stream_adaptor{[predicate = std::forward<Func>(predicate)]<typename T>(auto &&stream) mutable
                                               { return find_first<T>(std::move(predicate))(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Func>
    auto find_last(Func &&predicate)
    {
        return detail::inferred_stream_adaptor{[predicate = std::forward<Func>(predicate)]<typename T>(auto &&stream) mutable
                                               { return find_last<T>(std::move(predicate))(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Func>
    auto any_matches(Func &&predicate)
    {
        return detail::inferred_stream_adaptor{[predicate = std::forward<Func>(predicate)]<typename T>(auto &&stream) mutable
                                               { return any_matches<T>(std::move(predicate))(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Func>
    auto all_matches(Func &&predicate)
    {
        return detail::inferred_stream_adaptor{[predicate = std::forward<Func>(predicate)]<typename T>(auto &&stream) mutable
                                               { return all_matches<T>(std::move(predicate))(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Func>
    auto none_matches(Func &&predicate)
    {
        return detail::inferred_stream_adaptor{[predicate = std::forward<Func>(predicate)]<typename T>(auto &&stream) mutable
                                               { return none_matches<T>(std::move(predicate))(std::forward<decltype(stream)>(stream)); }};
    }

    inline auto limit(size_t count)
    {
        return detail::inferred_stream_adaptor{[count]<typename T>(auto &&stream)
                                               { return limit<T>(count)(std::forward<decltype(stream)>(stream)); }};
    }

    inline auto skip(size_t count)
    {
        return detail::inferred_stream_adaptor{[count]<typename T>(auto &&stream)
                                               { return skip<T>(count)(std::forward<decltype(stream)>(stream)); }};
    }

    inline auto min()
    {
        return detail::inferred_stream_adaptor{[]<typename T>(auto &&stream)
                                               { return min<T>()(std::forward<decltype(stream)>(stream)); }};
    }

    inline auto max()
    {
        return detail::inferred_stream_adaptor{[]<typename T>(auto &&stream)
                                               { return max<T>()(std::forward<decltype(stream)>(stream)); }};
    }

    inline auto sum()
    {
        return detail::inferred_stream_adaptor{[]<typename T>(auto &&stream)
                                               { return sum<T>()(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Stream>
    auto pipe(Stream &target)
    {
        return detail::inferred_stream_adaptor{[&target]<typename T>(auto &&stream)
                                               { return pipe<T>(target)(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Stream>
    auto forward_to(Stream &target)
    {
        return detail::inferred_stream_adaptor{[&target]<typename T>(auto &&stream)
                                               { return forward_to<T>(target)(std::forward<decltype(stream)>(stream)); }};
    }

    template <typename Collector>
    auto collect(Collector &&fn)
    {
        return detail::inferred_stream_adaptor{[fn = std::forward<Collector>(fn)]<typename T>(auto &&stream) mutable
                                               {
                                                   using Result = awaitable_resume_t<std::invoke_result_t<Collector, async_generator<T>>>;
                                                   return collect<Result, T>(std::move(fn))(std::forward<decltype(stream)>(stream));
                                               }};
    }

}
