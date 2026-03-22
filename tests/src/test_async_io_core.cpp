///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#define TEST_SUITE_NAME AsyncIOCoreTestSuite

#include "test_suite.hpp"
#include <webcraft/async/async.hpp>
#include <webcraft/async/io/core.hpp>
#include <string>
#include <vector>
#include <queue>
#include <span>
#include <unordered_set>
#include <ranges>

using namespace webcraft::async;
using namespace webcraft::async::io;
using namespace std::chrono_literals;

template <typename T>
class mock_readable_stream
{
public:
    explicit mock_readable_stream(std::vector<T> values) : values(std::move(values)) {}

    task<std::optional<T>> recv()
    {
        if (values.empty())
        {
            co_return std::nullopt;
        }
        T value = std::move(values.front());
        values.erase(values.begin());
        co_return std::make_optional(std::move(value));
    }

private:
    std::vector<T> values;
};

template <typename T>
class mock_writable_stream
{
private:
    std::queue<T> received_values;

public:
    task<bool> send(T value)
    {
        received_values.push(value);
        co_return true;
    }

    bool received(T value)
    {
        auto received = received_values.front();
        received_values.pop();
        return received == value;
    }
};

static_assert(async_readable_stream<mock_readable_stream<int>, int>, "mock_readable_stream should be an async readable stream");
static_assert(async_writable_stream<mock_writable_stream<int>, int>, "mock_writable_stream should be an async writable stream");
static_assert(async_readable_stream<mock_readable_stream<std::string>, std::string>, "mock_readable_stream should be an async readable stream");
static_assert(async_writable_stream<mock_writable_stream<std::string>, std::string>, "mock_writable_stream should be an async writable stream");

TEST_CASE(TestReadableStreamRead)
{
    mock_readable_stream<int> stream({1, 2, 3});

    auto task_fn = [&]() -> task<void>
    {
        std::vector<int> results;
        while (auto value = co_await stream.recv())
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 3) << "Should read three values from the stream";
        EXPECT_EQ(results[0], 1) << "First value should be 1";
        EXPECT_EQ(results[1], 2) << "Second value should be 2";
        EXPECT_EQ(results[2], 3) << "Third value should be 3";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestWritableStreamSend)
{
    mock_writable_stream<int> stream;

    auto task_fn = [&]() -> task<void>
    {
        EXPECT_TRUE(co_await stream.send(42)) << "Stream should accept the value 42";
        EXPECT_TRUE(co_await stream.send(84)) << "Stream should accept the value 84";
        EXPECT_TRUE(stream.received(42)) << "Stream should have received 42";
        EXPECT_TRUE(stream.received(84)) << "Stream should have received 84";

        EXPECT_TRUE(co_await stream.send(100)) << "Stream should accept the value 100";
        EXPECT_TRUE(stream.received(100)) << "Stream should have received 100";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestGeneratorFromReadableStream)
{
    mock_readable_stream<std::string> stream({"Hello", "World", "!"});

    auto task_fn = [&]() -> task<void>
    {
        auto gen = to_async_generator<std::string>(std::move(stream));
        std::vector<std::string> results;

        for_each_async(value, gen,
                       {
                           results.push_back(std::move(value));
                       });

        EXPECT_EQ(results.size(), 3) << "Should yield three values from the generator";
        EXPECT_EQ(results[0], "Hello") << "First value should be 'Hello'";
        EXPECT_EQ(results[1], "World") << "Second value should be 'World'";
        EXPECT_EQ(results[2], "!") << "Third value should be '!'";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestExternalRecv)
{
    mock_readable_stream<int> stream({1, 2, 3});

    auto task_fn = [&]() -> task<void>
    {
        std::vector<int> results;
        while (auto value = co_await recv<int>(stream))
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 3) << "Should read three values from the stream";
        EXPECT_EQ(results[0], 1) << "First value should be 1";
        EXPECT_EQ(results[1], 2) << "Second value should be 2";
        EXPECT_EQ(results[2], 3) << "Third value should be 3";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestExternalSend)
{
    mock_writable_stream<int> stream;

    auto task_fn = [&]() -> task<void>
    {
        EXPECT_TRUE(co_await send<int>(stream, 42)) << "Stream should accept the value 42";
        EXPECT_TRUE(co_await send<int>(stream, 84)) << "Stream should accept the value 84";
        EXPECT_TRUE(stream.received(42)) << "Stream should have received 42";
        EXPECT_TRUE(stream.received(84)) << "Stream should have received 84";

        EXPECT_TRUE(co_await send<int>(stream, 100)) << "Stream should accept the value 100";
        EXPECT_TRUE(stream.received(100)) << "Stream should have received 100";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestMultipleSend)
{
    mock_writable_stream<int> stream;

    auto task_fn = [&]() -> task<void>
    {
        std::vector<int> values = {1, 2, 3, 4, 5};
        std::span<int, 5> span(values);
        size_t sent_count = co_await send(stream, span);
        EXPECT_EQ(sent_count, 5) << "Should send all 5 values to the stream";
        for (auto i : values)
        {
            EXPECT_TRUE(stream.received(i)) << "Stream should have received value " << i;
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestMultipleRecv)
{
    std::vector<int> values = {1, 2, 3, 4, 5};
    mock_readable_stream<int> stream({1, 2, 3, 4, 5});

    auto task_fn = [&]() -> task<void>
    {
        std::array<int, 5> set_v;
        std::span<int, 5> span(set_v);
        size_t received_count = co_await recv(stream, span);
        EXPECT_EQ(received_count, 5) << "Should receive all 5 values from the stream";
        for (int i = 0; i < received_count; ++i)
        {
            EXPECT_EQ(values[i], span[i]) << "Value at index " << i << " should match";
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestChannelsInt)
{
    std::vector<int> values{1, 2, 3, 4, 5};
    auto [reader, writer] = make_mpsc_channel<int>();

    auto producer_fn = [&]() -> task<void>
    {
        for (auto &&value : values)
        {
            auto check = co_await writer.send(std::move(value));
            EXPECT_TRUE(check) << "Should have been successfully sent";
        }
    };

    auto consumer_fn = [&]() -> task<void>
    {
        for (auto value : values)
        {
            auto checker = co_await reader.recv();
            EXPECT_EQ(checker, value) << "Should have been the same";
        }
    };

    sync_wait(when_all(producer_fn(), consumer_fn()));
}

TEST_CASE(GeneratorToReadableStream)
{
    std::vector<int> values = {1, 2, 3, 4, 5};

    auto gen = [&]() -> async_generator<int>
    {
        for (auto value : values)
        {
            co_yield value;
        }
    };

    auto task_fn = [&]() -> task<void>
    {
        auto readable_stream = to_readable_stream(gen());

        size_t count = 0;
        while (auto value = co_await readable_stream.recv())
        {
            EXPECT_EQ(value, values[count]) << "Values should be equal";
            count++;
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(MpScChannel)
{
    auto [reader, writer] = make_mpsc_channel<std::string>();

    auto producer_one = [writer]() mutable -> task<void>
    {
        auto check = co_await writer.send("producer-1:alpha");
        EXPECT_TRUE(check) << "Producer one send should succeed";
        check = co_await writer.send("producer-1:beta");
        EXPECT_TRUE(check) << "Producer one send should succeed";
    };

    auto producer_two = [writer]() mutable -> task<void>
    {
        auto check = co_await writer.send("producer-2:gamma");
        EXPECT_TRUE(check) << "Producer two send should succeed";
        check = co_await writer.send("producer-2:delta");
        EXPECT_TRUE(check) << "Producer two send should succeed";
    };

    auto producer_three = [writer]() mutable -> task<void>
    {
        auto check = co_await writer.send("producer-3:epsilon");
        EXPECT_TRUE(check) << "Producer three send should succeed";
        check = co_await writer.send("producer-3:zeta");
        EXPECT_TRUE(check) << "Producer three send should succeed";
    };

    auto run_producers_and_close = [&]() -> task<void>
    {
        co_await when_all(producer_one(), producer_two(), producer_three());
        co_await writer.close();
    };

    std::unordered_set<std::string> expected_messages = {
        "producer-1:alpha",
        "producer-1:beta",
        "producer-2:gamma",
        "producer-2:delta",
        "producer-3:epsilon",
        "producer-3:zeta"};

    auto consumer_fn = [&]() -> task<void>
    {
        while (true)
        {
            auto checker = co_await reader.recv();
            if (!checker.has_value())
            {
                break;
            }
            auto erased = expected_messages.erase(*checker);
            EXPECT_EQ(erased, 1) << "Received unexpected or duplicate message";
        }

        EXPECT_TRUE(expected_messages.empty()) << "Consumer should receive every produced message before close";
    };

    sync_wait(when_all(run_producers_and_close(), consumer_fn()));
}

TEST_CASE(ThreadSafeMpScChannel)
{
    auto [reader, writer] = make_mpsc_channel<std::string>();

    thread_pool pool;

    struct threaded_awaitable
    {
        thread_pool &pool;

        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            pool.submit([h]()
                        { h.resume(); });
        }

        void await_resume() noexcept {}
    };

    auto producer_one = [writer, &pool](std::string message1) mutable -> task<void>
    {
        co_await threaded_awaitable{pool};
        auto check = co_await writer.send(message1);
        EXPECT_TRUE(check) << "Producer one send should succeed";
    };

    auto expected_messages_range = std::views::iota(1, 1001) | std::views::transform([](int value)
                                                                                     { return std::to_string(value); });

    std::unordered_set<std::string> expected_messages(
        expected_messages_range.begin(),
        expected_messages_range.end());

    auto run_producers_and_close = [&]() -> task<void>
    {
        std::vector<task<void>> producers;

        std::vector<std::string> messages{expected_messages.begin(), expected_messages.end()};

        for (const auto &message : messages)
        {
            producers.push_back(producer_one(message));
        }

        co_await when_all(producers);
        co_await writer.close();
    };

    auto consumer_fn = [&]() -> task<void>
    {
        while (true)
        {
            auto checker = co_await reader.recv();
            if (!checker.has_value())
            {
                break;
            }
            auto erased = expected_messages.erase(*checker);
            EXPECT_EQ(erased, 1) << "Received unexpected or duplicate message";
        }

        EXPECT_TRUE(expected_messages.empty()) << "Consumer should receive every produced message before close";
    };

    sync_wait(when_all(run_producers_and_close(), consumer_fn()));
}