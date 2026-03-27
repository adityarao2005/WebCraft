#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/io/core.hpp
 * @brief Core async stream concepts and adapters.
 *
 * Defines the readable/writable stream concepts used by WebCraft async I/O,
 * plus helper wrappers for integrating coroutine generators with streams.
 */

#include <utility>
#include <concepts>
#include <optional>
#include <coroutine>
#include <webcraft/async/task.hpp>
#include <webcraft/async/generator.hpp>
#include <webcraft/async/async_generator.hpp>
#include <mutex>
#include <queue>
#include <span>

namespace webcraft::async::io
{
    /** @brief Constraint that excludes `void` element types. */
    template <typename T>
    concept non_void_v = !std::is_void_v<T>;

    /** @brief Concept for streams exposing `recv()` that yields optional values. */
    template <typename Derived, typename R>
    concept async_readable_stream = std::is_move_constructible_v<Derived> && requires(Derived &stream) {
        { stream.recv() } -> std::same_as<task<std::optional<R>>>;
    };

    /** @brief Concept for readable streams that also support buffered receives. */
    template <typename Derived, typename R>
    concept async_buffered_readable_stream = async_readable_stream<Derived, R> && requires(Derived &stream, std::span<R> buffer) {
        { stream.recv(buffer) } -> std::same_as<task<std::size_t>>;
    };

    /** @brief Concept for streams exposing `send(value)` operations. */
    template <typename Derived, typename R>
    concept async_writable_stream = std::is_move_constructible_v<Derived> && requires(Derived &stream, R value) {
        { stream.send(value) } -> std::same_as<task<bool>>;
    };

    /** @brief Concept for writable streams that also support buffered sends. */
    template <typename Derived, typename R>
    concept async_buffered_writable_stream = async_writable_stream<Derived, R> && requires(Derived &stream, std::span<R> buffer) {
        { stream.send(buffer) } -> std::same_as<task<size_t>>;
    };

    /** @brief Concept for streams that provide asynchronous `close()`. */
    template <typename Derived, typename R>
    concept async_closeable_stream = (async_readable_stream<Derived, R> || async_writable_stream<Derived, R>) && requires(Derived t) {
        { t.close() } -> std::same_as<task<void>>;
    };

    /** @brief Receives the next element from a readable stream. */
    template <typename R>
    auto recv(async_readable_stream<R> auto &stream)
    {
        return stream.recv();
    }

    /** @brief Receives up to `buffer.size()` elements into a caller-provided buffer. */
    template <typename R, async_readable_stream<R> RStream, size_t BufferSize>
    task<std::size_t> recv(RStream &stream, std::span<R, BufferSize> buffer)
    {
        if constexpr (async_buffered_readable_stream<RStream, R>)
        {
            co_return co_await stream.recv(buffer);
        }
        else
        {
            size_t count = 0;
            size_t size = buffer.size();
            while (count < size)
            {
                auto value = co_await stream.recv();
                if (!value.has_value())
                {
                    break;
                }
                buffer[count++] = std::move(value.value());
            }
            co_return count;
        }
    }

    /** @brief Sends one value to a writable stream. */
    template <typename R, async_writable_stream<R> WStream>
    task<bool> send(WStream &stream, R &&value)
    {
        co_return co_await stream.send(std::forward<R>(value));
    }

    /** @brief Sends values from a caller-provided buffer to a writable stream. */
    template <typename R, async_writable_stream<R> WStream, size_t BufferSize>
    task<size_t> send(WStream &stream, std::span<R, BufferSize> buffer)
    {
        if constexpr (async_buffered_writable_stream<WStream, R>)
        {
            co_return co_await stream.send(buffer);
        }
        else
        {
            size_t count = 0;
            size_t size = buffer.size();
            while (count < size)
            {
                auto sent = co_await stream.send(std::move(buffer[count]));
                if (!sent)
                {
                    break;
                }
                count++;
            }
            co_return count;
        }
    }

    /** @brief Wraps a readable stream as an async generator. */
    template <typename R, async_readable_stream<R> RStream>
    async_generator<R> to_async_generator(RStream &&stream)
    {
        while (true)
        {
            auto value = co_await stream.recv();
            if (!value.has_value())
            {
                break;
            }
            co_yield std::move(value.value());
        }
        co_return;
    }

    /** @brief Wraps an async generator with the readable stream interface. */
    template <typename R>
    async_readable_stream<R> auto to_readable_stream(async_generator<R> &&gen)
    {
        struct stream
        {
            async_generator<R> gen;
            std::optional<typename async_generator<R>::iterator> it;

            explicit stream(async_generator<R> &&gen) : gen(std::move(gen)) {}

            task<std::optional<R>> recv()
            {
                if (!it.has_value())
                {
                    it = co_await gen.begin();
                }

                if (it == gen.end())
                {
                    co_return std::nullopt;
                }

                auto &itr = *it;
                std::optional<R> value = std::move(*itr);
                co_await ++itr;
                co_return std::move(value);
            }
        };

        static_assert(async_readable_stream<stream, R>, "Should be an async_readable_stream");

        return stream{std::move(gen)};
    };

    namespace detail
    {
        template <non_void_v T>
        struct mpsc_channel_subscription
        {
            std::mutex state_mutex;
            std::coroutine_handle<> continuation;
            std::queue<T> values;
            bool closed = false;

            task<std::optional<T>> get_next()
            {
                struct awaitable
                {
                    mpsc_channel_subscription<T> &sub;

                    bool await_ready() const noexcept
                    {
                        std::scoped_lock lock(sub.state_mutex);
                        return !sub.values.empty() || sub.closed;
                    }

                    bool await_suspend(std::coroutine_handle<> h) noexcept
                    {
                        std::scoped_lock lock(sub.state_mutex);
                        if (!sub.values.empty() || sub.closed)
                        {
                            return false;
                        }
                        sub.continuation = h;
                        return true;
                    }

                    std::optional<T> await_resume() noexcept
                    {
                        std::scoped_lock lock(sub.state_mutex);
                        if (sub.closed && sub.values.empty())
                        {
                            return std::nullopt;
                        }
                        auto value = std::move(sub.values.front());
                        sub.values.pop();
                        return std::move(value);
                    }
                };

                co_return co_await awaitable{*this};
            }

            task<bool> send(T val)
            {
                std::coroutine_handle<> to_resume;
                {
                    std::scoped_lock lock(state_mutex);
                    if (closed)
                    {
                        co_return false;
                    }

                    values.push(std::move(val));
                    to_resume = std::exchange(continuation, {});
                }

                if (to_resume)
                {
                    to_resume.resume();
                }
                co_return true;
            }

            void close()
            {
                std::coroutine_handle<> to_resume;
                {
                    std::scoped_lock lock(state_mutex);
                    closed = true;
                    to_resume = std::exchange(continuation, {});
                }

                if (to_resume)
                {
                    to_resume.resume();
                }
            }
        };

        template <non_void_v T>
        struct mpsc_channel_rstream
        {
        private:
            std::shared_ptr<mpsc_channel_subscription<T>> subscription;

        public:
            explicit mpsc_channel_rstream(std::shared_ptr<mpsc_channel_subscription<T>> sub) noexcept
                : subscription(sub)
            {
            }

            task<std::optional<T>> recv()
            {
                return subscription->get_next();
            }
        };

        template <non_void_v T>
        struct mpsc_channel_wstream
        {
        private:
            std::shared_ptr<mpsc_channel_subscription<T>> subscription;

        public:
            explicit mpsc_channel_wstream(std::shared_ptr<mpsc_channel_subscription<T>> sub) noexcept
                : subscription(sub)
            {
            }

            task<bool> send(T val)
            {
                return subscription->send(std::move(val));
            }

            task<void> close()
            {
                subscription->close();
                co_return;
            }
        };

        static_assert(async_readable_stream<mpsc_channel_rstream<int>, int>, "mpsc_channel_rstream should be an async readable stream");
        static_assert(async_writable_stream<mpsc_channel_wstream<int>, int>, "mpsc_channel_wstream should be an async writable stream");
        static_assert(async_closeable_stream<mpsc_channel_wstream<int>, int>, "mpsc_channel_wstream should be an async closeable stream");
        static_assert(async_readable_stream<mpsc_channel_rstream<std::string>, std::string>, "mpsc_channel_rstream should be an async readable stream");
        static_assert(async_writable_stream<mpsc_channel_wstream<std::string>, std::string>, "mpsc_channel_wstream should be an async writable stream");
        static_assert(async_closeable_stream<mpsc_channel_wstream<std::string>, std::string>, "mpsc_channel_wstream should be an async closeable stream");
    }

    /**
     * @brief Creates an in-memory single-consumer async channel.
     * @return Pair of readable and writable stream endpoints.
     */
    template <non_void_v T>
    std::pair<detail::mpsc_channel_rstream<T>, detail::mpsc_channel_wstream<T>> make_mpsc_channel()
    {
        auto subscription = std::make_shared<detail::mpsc_channel_subscription<T>>();
        return {detail::mpsc_channel_rstream<T>(subscription), detail::mpsc_channel_wstream<T>(subscription)};
    }
}
