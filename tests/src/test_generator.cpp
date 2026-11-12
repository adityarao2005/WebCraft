///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#define TEST_SUITE_NAME GeneratorTestSuite

#include "test_suite.hpp"
#include <tuple>
#include <vector>
#include <webcraft/async/async.hpp>

using namespace webcraft::ranges;
using namespace webcraft::async;

TEST_CASE(ReturnsCopyTest)
{
    auto f = []() -> generator<float>
    {
        co_yield 1.0f;
        co_yield 2.0f;
    };

    auto gen = f();
    auto iter = gen.begin();

    EXPECT_TRUE(*iter == 1.0f);
    ++iter;
    EXPECT_TRUE(*iter == 2.0f);
    ++iter;
    EXPECT_TRUE(iter == gen.end());
}

TEST_CASE(ReturnsReferenceTest)
{
    auto f = [](float &value) -> generator<float &>
    {
        co_yield value;
    };

    float value = 1.0f;
    for (auto &x : f(value))
    {
        EXPECT_TRUE(&x == &value);
        x += 1.0f;
    }

    EXPECT_TRUE(value == 2.0f);
}

TEST_CASE(ConstTypeTest)
{
    auto fib = []() -> generator<const std::uint64_t>
    {
        std::uint64_t a = 0, b = 1;
        while (true)
        {
            co_yield b;
            b += std::exchange(a, b);
        }
    };

    std::uint64_t count = 0;
    for (auto i : fib())
    {
        if (i > 1'000'000)
        {
            break;
        }
        ++count;
    }

    // 30th fib number is 832'040
    EXPECT_TRUE(count == 30);
}

TEST_CASE(LazynessTest)
{
    bool reachedA = false;
    bool reachedB = false;
    bool reachedC = false;
    auto f = [&]() -> generator<int>
    {
        reachedA = true;
        co_yield 1;
        reachedB = true;
        co_yield 2;
        reachedC = true;
    };

    auto gen = f();
    EXPECT_TRUE(!reachedA);
    auto iter = gen.begin();
    EXPECT_TRUE(reachedA);
    EXPECT_TRUE(!reachedB);
    EXPECT_TRUE(*iter == 1);
    ++iter;
    EXPECT_TRUE(reachedB);
    EXPECT_TRUE(!reachedC);
    EXPECT_TRUE(*iter == 2);
    ++iter;
    EXPECT_TRUE(reachedC);
    EXPECT_TRUE(iter == gen.end());
}

TEST_CASE(ThrowingBeforeYieldThrowsOnBegin)
{
    class X
    {
    };

    auto g = []() -> webcraft::ranges::generator<int>
    {
        throw X{};
        co_return;
    }();

    EXPECT_THROW(g.begin(), X) << "should throw X when trying to advance past the first element";
}

TEST_CASE(ThrowingAfterYieldThrowsOnNext)
{
    class X
    {
    };

    auto g = []() -> webcraft::ranges::generator<int>
    {
        co_yield 1;
        throw X{};
    }();

    auto iter = g.begin();
    EXPECT_TRUE(iter != g.end());
    EXPECT_THROW(++iter, X) << "should throw X when trying to advance past the first element";
}

namespace
{
    template <typename FIRST, typename SECOND>
    auto concat(FIRST &&first, SECOND &&second)
    {
        using value_type = std::remove_reference_t<decltype(*first.begin())>;
        return [](FIRST first, SECOND second) -> webcraft::ranges::generator<value_type>
        {
            for (auto &&x : first)
                co_yield x;
            for (auto &&y : second)
                co_yield y;
        }(std::forward<FIRST>(first), std::forward<SECOND>(second));
    }
}

TEST_CASE(SafeCaptureOfRValueReferenceArgs)
{
    using namespace std::string_literals;

    // EXPECT_TRUE that we can capture l-values by reference and that temporary
    // values are moved into the coroutine frame.
    std::string byRef = "bar";
    auto g = concat("foo"s, concat(byRef, std::vector<char>{'b', 'a', 'z'}));

    byRef = "buzz";

    std::string s;
    for (char c : g)
    {
        s += c;
    }

    EXPECT_TRUE(s == "foobuzzbaz");
}

namespace
{
    webcraft::ranges::generator<int> range(int start, int end)
    {
        for (; start < end; ++start)
        {
            co_yield start;
        }
    }
}

namespace
{
    template <std::size_t window, typename Range>
    webcraft::ranges::generator<const double> low_pass(Range rng)
    {
        auto it = std::begin(rng);
        const auto itEnd = std::end(rng);

        const double invCount = 1.0 / window;
        double sum = 0;

        using iter_cat =
            typename std::iterator_traits<decltype(it)>::iterator_category;

        if constexpr (std::is_base_of_v<std::random_access_iterator_tag, iter_cat>)
        {
            for (std::size_t count = 0; it != itEnd && count < window; ++it)
            {
                sum += *it;
                ++count;
                co_yield sum / count;
            }

            for (; it != itEnd; ++it)
            {
                sum -= *(it - window);
                sum += *it;
                co_yield sum *invCount;
            }
        }
        else if constexpr (std::is_base_of_v<std::forward_iterator_tag, iter_cat>)
        {
            auto windowStart = it;
            for (std::size_t count = 0; it != itEnd && count < window; ++it)
            {
                sum += *it;
                ++count;
                co_yield sum / count;
            }

            for (; it != itEnd; ++it, ++windowStart)
            {
                sum -= *windowStart;
                sum += *it;
                co_yield sum *invCount;
            }
        }
        else
        {
            // Just assume an input iterator
            double buffer[window];

            for (std::size_t count = 0; it != itEnd && count < window; ++it)
            {
                buffer[count] = *it;
                sum += buffer[count];
                ++count;
                co_yield sum / count;
            }

            for (std::size_t pos = 0; it != itEnd; ++it, pos = (pos + 1 == window) ? 0 : (pos + 1))
            {
                sum -= std::exchange(buffer[pos], *it);
                sum += buffer[pos];
                co_yield sum *invCount;
            }
        }
    }
}

TEST_CASE(TestRangesInterop)
{
    auto gen_fn = []() -> webcraft::ranges::generator<int>
    {
        for (int i = 0; i < 10; ++i)
        {
            co_yield i;
        }
    };

    auto range = gen_fn() | std::views::transform([](int i)
                                                  { return i * 2; }) |
                 std::views::drop_while([](int i)
                                        { return i < 10; });

    for (auto &&value : range)
    {
        EXPECT_TRUE(value % 2 == 0) << "Value should be even";
        EXPECT_TRUE(value >= 10) << "Value should be greater than or equal to 10";
    }
}

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

TEST_CASE(TestEmptyAsyncGenerator)
{
    sync_wait([]() -> task<>
              {
		// Iterating over default-constructed async_generator just
		// gives an empty sequence.
		async_generator<int> g;
		auto it = co_await g.begin();
		EXPECT_EQ(it, g.end()) << "Iterator must be equal to end"; }());
}

TEST_CASE(TestAsyncGeneratorLazyness)
{
    bool startedExecution = false;
    {
        auto gen = [&]() -> async_generator<int>
        {
            startedExecution = true;
            co_yield 1;
        }();
        EXPECT_FALSE(startedExecution);
    }
    EXPECT_FALSE(startedExecution);
}

TEST_CASE(TestEnumerateOneValueAsyncGenerator)
{
    sync_wait([]() -> task<>
              {
		bool startedExecution = false;
		auto makeGenerator = [&]() -> async_generator<std::uint32_t>
		{
			startedExecution = true;
			co_yield 1;
		};

		auto gen = makeGenerator();

		EXPECT_FALSE(startedExecution);

		auto it = co_await gen.begin();

		EXPECT_TRUE(startedExecution);
		EXPECT_TRUE(it != gen.end());
		EXPECT_TRUE(*it == 1u);
		EXPECT_TRUE(co_await ++it == gen.end()); }());
}

TEST_CASE(TestEnumerateMultipleValuesAsyncGenerator)
{
    sync_wait([]() -> task<>
              {
		bool startedExecution = false;
		auto makeGenerator = [&]() -> async_generator<std::uint32_t>
		{
			startedExecution = true;
			co_yield 1;
			co_yield 2;
			co_yield 3;
		};

		auto gen = makeGenerator();

		EXPECT_FALSE(startedExecution);

		auto it = co_await gen.begin();

		EXPECT_TRUE(startedExecution);

		EXPECT_TRUE(it != gen.end());
		EXPECT_TRUE(*it == 1u);

		EXPECT_TRUE(co_await ++it != gen.end());
		EXPECT_TRUE(*it == 2u);

		EXPECT_TRUE(co_await ++it != gen.end());
		EXPECT_TRUE(*it == 3u);

		EXPECT_TRUE(co_await ++it == gen.end()); }());
}

namespace
{
    class set_to_true_on_destruction
    {
    public:
        set_to_true_on_destruction(bool *value)
            : m_value(value)
        {
        }

        set_to_true_on_destruction(set_to_true_on_destruction &&other)
            : m_value(other.m_value)
        {
            other.m_value = nullptr;
        }

        ~set_to_true_on_destruction()
        {
            if (m_value != nullptr)
            {
                *m_value = true;
            }
        }

        set_to_true_on_destruction(const set_to_true_on_destruction &) = delete;
        set_to_true_on_destruction &operator=(const set_to_true_on_destruction &) = delete;

    private:
        bool *m_value;
    };
}

TEST_CASE(TestAsyncGeneratorDestruction)
{
    sync_wait([]() -> task<>
              {
		bool aDestructed = false;
		bool bDestructed = false;

		auto makeGenerator = [&](set_to_true_on_destruction a) -> async_generator<std::uint32_t>
		{
			set_to_true_on_destruction b(&bDestructed);
			co_yield 1;
			co_yield 2;
		};

		{
			auto gen = makeGenerator(&aDestructed);

			EXPECT_FALSE(aDestructed);
			EXPECT_FALSE(bDestructed);

			auto it = co_await gen.begin();
			EXPECT_FALSE(aDestructed);
			EXPECT_FALSE(bDestructed);
			EXPECT_TRUE(*it == 1u);
		}

		EXPECT_TRUE(aDestructed);
		EXPECT_TRUE(bDestructed); }());
}

TEST_CASE(TestAsyncProducerConsumer)
{
    async_event p1;
    async_event p2;
    async_event p3;
    async_event c1;

    auto produce = [&]() -> async_generator<std::uint32_t>
    {
        co_await p1;
        co_yield 1;
        co_await p2;
        co_yield 2;
        co_await p3;
    };

    bool consumerFinished = false;

    auto consume = [&]() -> task<>
    {
        auto generator = produce();
        auto it = co_await generator.begin();
        EXPECT_TRUE(*it == 1u);
        (void)co_await ++it;
        EXPECT_TRUE(*it == 2u);
        co_await c1;
        (void)co_await ++it;
        EXPECT_TRUE(it == generator.end());
        consumerFinished = true;
    };

    auto unblock = [&]() -> task<>
    {
        p1.set();
        p2.set();
        c1.set();
        EXPECT_FALSE(consumerFinished);
        p3.set();
        EXPECT_TRUE(consumerFinished);
        co_return;
    };

    std::vector<task<void>> tasks;
    tasks.emplace_back(consume());
    tasks.emplace_back(unblock());
    sync_wait(when_all(tasks));
}

TEST_CASE(TestExceptionAsyncGeneratorBegin)
{
    class TestException
    {
    };
    auto gen = [](bool shouldThrow) -> async_generator<std::uint32_t>
    {
        if (shouldThrow)
        {
            throw TestException();
        }
        co_yield 1;
    }(true);

    sync_wait([&]() -> task<>
              { EXPECT_THROW(co_await gen.begin(), TestException) << "Should have thrown"; }());
}

TEST_CASE(TestExceptionAsyncGeneratorIncrement)
{
    class TestException
    {
    };
    auto gen = [](bool shouldThrow) -> async_generator<std::uint32_t>
    {
        co_yield 1;
        if (shouldThrow)
        {
            throw TestException();
        }
    }(true);

    sync_wait([&]() -> task<>
              {
		auto it = co_await gen.begin();
		EXPECT_TRUE(*it == 1u);
		EXPECT_THROW(co_await ++it, TestException) << "Should have thrown";
		EXPECT_TRUE(it == gen.end()); }());
}

TEST_CASE(TestAsyncGeneratorThroughput)
{
    constexpr std::uint32_t count = 1'000'000u;
    auto makeSequence = [](async_event &event) -> async_generator<std::uint32_t>
    {
        for (std::uint32_t i = 0; i < count; ++i)
        {
            if (i == count / 2)
            {
                co_await event;
            }
            co_yield i;
        }
    };

    bool finished = false;
    auto consumer = [&](async_generator<std::uint32_t> sequence) -> task<>
    {
        std::uint32_t expected = 0;
        for_each_async(value, sequence,
                       {
                           EXPECT_EQ(value, expected++);
                       });
        EXPECT_EQ(expected, count);
        finished = true;
    };

    async_event event;
    auto work = consumer(makeSequence(event));
    EXPECT_FALSE(finished);
    event.set();
    sync_wait(std::move(work));
    EXPECT_TRUE(finished);
}

TEST_CASE(TestAsyncGeneratorCrossThreadHandoff)
{
    // Repeatedly race completion against registration of consumer suspension.
    struct resume_on_thread
    {
        std::thread &worker;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle)
        {
            // The new thread may destroy this awaiter before std::thread's
            // constructor returns. Cache the destination before publishing
            // the handle, and never access the awaiter after publication.
            auto *destination = std::addressof(worker);
            *destination = std::thread([handle] { handle.resume(); });
        }
        void await_resume() const noexcept {}
    };

    for (int mode = 0; mode < 6; ++mode)
    {
        SCOPED_TRACE(mode);
        for (int round = 0; round < 100; ++round)
        {
            std::thread worker;
            bool destroyed = false;
            auto produce = [&]() -> async_generator<int>
            {
                set_to_true_on_destruction guard(&destroyed);
                // Exercise an asynchronous increment as well as begin().
                if (mode == 3 || mode == 4)
                {
                    co_yield 0;
                }
                co_await resume_on_thread{worker};
                if (mode == 1 || mode == 4)
                {
                    co_return;
                }
                if (mode == 2 || mode == 3)
                {
                    throw std::runtime_error("producer failed");
                }
                for (int i = 0; i < 1000; ++i)
                {
                    co_yield i;
                }
            };
            auto consume = [&]() -> task<>
            {
                auto sequence = produce();
                int expected = 0;
                bool caught = false;
                try
                {
                    auto it = co_await sequence.begin();
                    while (it != sequence.end())
                    {
                        EXPECT_EQ(*it, expected++);
                        if (mode == 5)
                        {
                            break;
                        }
                        co_await ++it;
                    }
                }
                catch (const std::runtime_error &error)
                {
                    caught = true;
                    EXPECT_STREQ(error.what(), "producer failed");
                }
                EXPECT_EQ(caught, mode == 2 || mode == 3);
                EXPECT_EQ(expected, mode == 0 ? 1000 :
                                    (mode == 1 || mode == 2 ? 0 : 1));
            };
            sync_wait(consume());
            worker.join();
            EXPECT_TRUE(destroyed);
        }
    }
}

TEST_CASE(TestAsyncGeneratorBatchedProcessing)
{
    // Test a batched approach that might avoid stack buildup
    auto makeSequence = [](std::uint32_t count, std::uint32_t batch_size = 1000) -> async_generator<std::vector<std::uint32_t>>
    {
        std::cout << "Batched generator starting with count: " << count << ", batch_size: " << batch_size << std::endl;

        for (std::uint32_t start = 0; start < count; start += batch_size)
        {
            std::vector<std::uint32_t> batch;
            std::uint32_t end = std::min(start + batch_size, count);

            for (std::uint32_t i = start; i < end; ++i)
            {
                batch.push_back(i);
            }

            std::cout << "Yielding batch: " << start << "-" << (end - 1) << " (size: " << batch.size() << ")" << std::endl;
            co_yield batch;
        }

        std::cout << "Batched generator finished" << std::endl;
    };

    auto consumer_batched = [](async_generator<std::vector<std::uint32_t>> sequence, std::uint32_t expected_count) -> task<>
    {
        std::cout << "Batched consumer starting, expecting " << expected_count << " items" << std::endl;
        std::uint32_t expected = 0;
        std::uint32_t total_count = 0;
        std::uint32_t batch_num = 0;

        for_each_async(it_value, sequence,
                       {
                           const auto &batch = it_value;

                           std::cout << "Processing batch " << batch_num++ << " with " << batch.size() << " items" << std::endl;

                           for (std::uint32_t value : batch)
                           {
                               EXPECT_TRUE(value == expected++) << "Value mismatch: got " << value << ", expected " << (expected - 1);
                               ++total_count;
                           }
                       });

        EXPECT_TRUE(total_count == expected_count) << "Expected " << expected_count << " but got " << total_count;
        std::cout << "Batched consumer finished. Total consumed: " << total_count << std::endl;
    };

    // Test with 100,000 items in batches of 1000
    std::cout << "\n=== Testing batched approach with 100000 items ===" << std::endl;
    try
    {
        sync_wait(consumer_batched(makeSequence(100000, 1000), 100000));
        std::cout << "✓ Batched approach with 100000 items completed successfully" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "✗ Batched approach with 100000 items failed: " << e.what() << std::endl;
        EXPECT_TRUE(false) << "Batched approach failed: " << e.what();
    }
}

TEST_CASE(TestGeneratorEarlyExitReleasesFrameArgument)
{
    std::weak_ptr<int> lifetime;
    auto produce = [](std::shared_ptr<int> owner) -> generator<int>
    {
        co_yield *owner;
        co_yield *owner + 1;
    };
    {
        auto owner = std::make_shared<int>(42);
        lifetime = owner;
        auto sequence = produce(std::move(owner));
        auto moved = std::move(sequence);
        auto it = moved.begin();
        EXPECT_EQ(*it, 42);
        EXPECT_FALSE(lifetime.expired());
        // Destroy while suspended at the first yield.
    }
    EXPECT_TRUE(lifetime.expired());
}

TEST_CASE(TestAsyncGeneratorExceptionAfterSuspensionReleasesState)
{
    async_event gate;
    std::weak_ptr<int> lifetime;
    auto produce = [](async_event &gate, std::shared_ptr<int> owner) -> async_generator<int>
    {
        co_yield *owner;
        co_await gate;
        throw std::runtime_error("after suspension");
    };
    auto consume = [](async_generator<int> sequence) -> task<void>
    {
        auto it = co_await sequence.begin();
        EXPECT_EQ(*it, 7);
        co_await ++it;
    };
    {
        auto owner = std::make_shared<int>(7);
        lifetime = owner;
        auto pending = consume(produce(gate, std::move(owner)));
        EXPECT_FALSE(pending.await_ready());
        EXPECT_FALSE(lifetime.expired());
        gate.set();
        EXPECT_THROW(sync_wait(pending), std::runtime_error);
    }
    EXPECT_TRUE(lifetime.expired());
}
