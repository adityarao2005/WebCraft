///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#ifdef __linux__

#define TEST_SUITE_NAME RuntimeQueueTestSuite

#include "test_suite.hpp"
#include <array>
#include <vector>
#include <webcraft/async/async.hpp>

using namespace webcraft::async;

TEST_CASE(TestRepeatedIdleWakeups)
{
    runtime_context context;
    std::atomic<unsigned> completed{0};
    auto work = [&]() -> task<void>
    {
        co_await yield();
        ++completed;
    };
    for (unsigned round = 0; round < 32; ++round)
    {
        sync_wait(work());
        EXPECT_EQ(completed.load(), round + 1);
        // Allow the runtime to become idle before the next external producer.
        std::this_thread::sleep_for(1ms);
    }
}

TEST_CASE(TestConcurrentProducerAndCallbackSubmissions)
{
    runtime_context context;
    constexpr std::size_t producer_count = 4;
    constexpr std::size_t tasks_per_producer = 128;
    std::array<std::atomic<unsigned>, producer_count * tasks_per_producer> completed{};

    auto work = [&](std::size_t index) -> task<void>
    {
        for (int step = 0; step < 4; ++step)
        {
            co_await yield();
        }
        completed[index].fetch_add(1, std::memory_order_relaxed);
    };

    std::vector<std::jthread> producers;
    for (std::size_t producer = 0; producer < producer_count; ++producer)
    {
        producers.emplace_back([&, producer]
                               {
                                   std::vector<task<void>> pending;
                                   pending.reserve(tasks_per_producer);
                                   for (std::size_t i = 0; i < tasks_per_producer; ++i)
                                   {
                                       pending.emplace_back(work(producer * tasks_per_producer + i));
                                   }
                                   sync_wait(when_all(pending));
                               });
    }
    for (auto &producer : producers)
    {
        producer.join();
    }
    for (const auto &count : completed)
    {
        EXPECT_EQ(count.load(), 1u);
    }
}

TEST_CASE(TestQueuedBurstBeyondRingCapacity)
{
    runtime_context context;
    constexpr std::size_t count = 2048; // The runtime ring has 1024 submission slots.
    std::array<std::atomic<unsigned>, count> completed{};
    event_signal entered;
    event_signal release;
    std::vector<task<void>> pending;
    pending.reserve(count);

    // Hold the submission thread so the entire burst is queued before it drains.
    detail::submit_runtime_operation([&](io_uring_sqe *sqe)
                                     {
                                         entered.set();
                                         release.wait();
                                         io_uring_prep_nop(sqe);
                                         io_uring_sqe_set_data64(sqe, 0);
                                     });
    entered.wait();

    auto work = [&](std::size_t index) -> task<void>
    {
        co_await yield();
        completed[index].fetch_add(1, std::memory_order_relaxed);
    };
    for (std::size_t i = 0; i < count; ++i)
    {
        pending.emplace_back(work(i));
    }
    release.set();
    sync_wait(when_all(pending));

    for (const auto &value : completed)
    {
        EXPECT_EQ(value.load(), 1u);
    }
}

#endif
