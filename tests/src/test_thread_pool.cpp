///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////


#define TEST_SUITE_NAME ThreadPoolTests

#include "test_suite.hpp"
#include <webcraft/async/thread_pool.hpp>
#include <future>
#include <vector>
#include <set>
#include <chrono>

using namespace webcraft::async;
using namespace std::chrono_literals;

TEST_CASE(RunOnDifferentThread)
{
    thread_pool thpr(2);
    EXPECT_EQ(thpr.get_workers_size(), 2) << "Thread pool should have 2 workers";

    auto id = std::this_thread::get_id();
    std::thread::id id2;
    event_signal sig;

    thpr.submit(
        [id, &id2, &sig]()
        {
            id2 = std::this_thread::get_id();
            sig.set();
        });

    sig.wait();
    EXPECT_NE(id, id2) << "The task should run on a different thread";
}

TEST_CASE(RunMultipleTasksOnMultipleThreads)
{
    constexpr size_t num_tasks = 4;
    thread_pool thpr(num_tasks, num_tasks);

    EXPECT_EQ(thpr.get_workers_size(), num_tasks) << "Thread pool should have the specified number of workers";

    auto id = std::this_thread::get_id();

    std::atomic<int> success_counter{0}, run_counter{0};
    std::mutex mtx;
    std::condition_variable cv;

    for (size_t i = 0; i < num_tasks; ++i)
    {
        thpr.submit(
            [id, &success_counter, &run_counter, &cv]()
            {
                auto id2 = std::this_thread::get_id();
                if (id != id2)
                {
                    ++success_counter;
                    ++run_counter;
                    cv.notify_all();
                }
            });
    }

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, 2s, [&run_counter, num_tasks]()
                    { return run_counter.load() == num_tasks; });
    }
    EXPECT_EQ(success_counter.load(), num_tasks) << "All tasks should run on different threads";
}

void takes_a_long_time_duration()
{
    std::this_thread::sleep_for(500ms);
}

TEST_CASE(RunMoreThanMinimumTasks)
{
    constexpr size_t min_workers = 4, num_tasks = 6, max_workers = 8;
    thread_pool thpr(min_workers, max_workers);
    EXPECT_EQ(thpr.get_workers_size(), min_workers) << "Should have the minimum amount of workers";
    EXPECT_EQ(thpr.get_available_workers(), min_workers) << "Should have the minimum amount of available workers";

    std::vector<std::future<void>> futures;

    for (size_t i = 0; i < min_workers; ++i)
    {
        EXPECT_EQ(thpr.get_workers_size(), min_workers) << "Before: Should have " << min_workers << " workers for iteration " << i;
        EXPECT_EQ(thpr.get_available_workers(), 4 - i) << "Before: Should have " << (4 - i) << " available workers";
        futures.push_back(thpr.submit(takes_a_long_time_duration));
        std::this_thread::sleep_for(10ms);
        EXPECT_EQ(thpr.get_workers_size(), min_workers) << "After: Should have " << min_workers << " workers for iteration " << i;
        EXPECT_EQ(thpr.get_available_workers(), 4 - i - 1) << "After: Should have " << (4 - i - 1) << " available workers";
    }

    for (size_t i = 0; i < num_tasks - min_workers; ++i)
    {
        EXPECT_EQ(thpr.get_workers_size(), min_workers + i) << "Before: Should have " << (min_workers + i) << " workers";
        EXPECT_EQ(thpr.get_available_workers(), 0) << "Before: Should have 0 available workers";
        futures.push_back(thpr.submit(takes_a_long_time_duration));
        std::this_thread::sleep_for(10ms);
        EXPECT_EQ(thpr.get_workers_size(), min_workers + i + 1) << "After: Should have " << (min_workers + i + 1) << " workers";
        EXPECT_EQ(thpr.get_available_workers(), 0) << "After: Should have 0 available workers";
    }

    EXPECT_EQ(thpr.get_workers_size(), num_tasks);

    for (auto &fut : futures)
    {
        fut.get();
    }
}

TEST_CASE(RunMoreThanMaxTasks)
{
    constexpr size_t min_workers = 4, max_workers = 8;
    constexpr auto idle_timeout = 200ms;
    thread_pool thpr(min_workers, max_workers, idle_timeout);
    EXPECT_EQ(thpr.get_workers_size(), min_workers) << "Should have the minimum amount of workers";
    EXPECT_EQ(thpr.get_available_workers(), min_workers) << "Should have the minimum amount of available workers";

    std::vector<std::future<void>> futures;

    for (size_t i = 0; i < max_workers; ++i)
    {
        futures.push_back(thpr.submit(takes_a_long_time_duration));
        std::this_thread::sleep_for(10ms);
    }

    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(thpr.get_workers_size(), max_workers) << "After: Should have " << max_workers << " workers";
    EXPECT_EQ(thpr.get_available_workers(), 0) << "After: Should have 0 available workers";

    futures.push_back(thpr.submit(takes_a_long_time_duration));
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(thpr.get_workers_size(), max_workers) << "After: Should have " << max_workers << " workers";
    EXPECT_EQ(thpr.get_available_workers(), 0) << "After: Should have 0 available workers";

    for (auto &fut : futures)
    {
        fut.get();
    }

    std::this_thread::sleep_for(idle_timeout);

    EXPECT_EQ(thpr.get_workers_size(), min_workers) << "After idle timeout: Should have " << min_workers << " workers";
    EXPECT_EQ(thpr.get_available_workers(), min_workers) << "After idle timeout: Should have " << min_workers << " available workers";
}

TEST_CASE(TestSubmitPropagatesExceptionAndWorkerRemainsUsable)
{
    thread_pool pool(1, 1);
    auto failed = pool.submit([]() -> int { throw std::runtime_error("worker failure"); });
    EXPECT_THROW(failed.get(), std::runtime_error);
    auto next = pool.submit([] { return std::make_unique<int>(42); });
    EXPECT_EQ(*next.get(), 42);
}
