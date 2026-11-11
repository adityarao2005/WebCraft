///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////


#define TEST_SUITE_NAME RuntimeTestSuite

#include "test_suite.hpp"
#include <webcraft/async/async.hpp>

using namespace webcraft::async;
using namespace std::chrono_literals;

TEST_CASE(TestRuntimeInitAndDestroy)
{
    runtime_context context;
}

TEST_CASE(TestSimpleRuntime)
{
    std::cout << "Starting TestSimpleRuntime..." << std::endl;
    runtime_context context;

    auto task_fn = []() -> task<void>
    {
        int count = 0;
        for (int i = 0; i < 10; ++i)
        {
            int temp = count;
            co_await yield();
            EXPECT_EQ(temp, count) << "Count should remain the same during yield";
            count++;
        }
        EXPECT_EQ(count, 10) << "Count should be 10 after yielding 10 times";
    };

    sync_wait(task_fn());

    std::cout << "TestSimpleRuntime completed successfully." << std::endl;
}

TEST_CASE(TestRuntimeTimerTask)
{
    std::cout << "Starting TestRuntimeTimerTask..." << std::endl;
    runtime_context context;

    auto timer_task = []() -> task<void>
    {
        for (int i = 0; i < 3; i++)
        {
            auto timer_duration = test_timer_timeout * (i + 1);
            auto start_time = std::chrono::steady_clock::now();
            co_await sleep_for(timer_duration);
            auto end_time = std::chrono::steady_clock::now();

            auto elapsed_time = end_time - start_time;
            EXPECT_GE(elapsed_time, timer_duration - test_adjustment_factor) << "Elapsed time should be greater than or equal to timer duration";
        }
    };

    sync_wait(timer_task());

    std::cout << "TestRuntimeTimerTask completed successfully." << std::endl;
}

TEST_CASE(TestRuntimeTimerCancellationTask)
{
    std::cout << "Starting TestRuntimeTimerCancellationTask..." << std::endl;
    runtime_context context;

    std::stop_source source;
    auto timer_task = [&source]() -> task<void>
    {
        std::stop_token token = source.get_token();

        auto start_time = std::chrono::steady_clock::now();

        co_await sleep_for(test_timer_timeout, token);
        auto end_time = std::chrono::steady_clock::now();

        auto elapsed_time = end_time - start_time;
        EXPECT_GE(elapsed_time, test_cancel_timeout) << "The timer should have been cancelled after " << test_cancel_timeout.count() << "ms";
    };

    auto pending = timer_task();
    // Cancellation resumes the coroutine on this worker. Keep its join owner on
    // the test thread, outside the coroutine frame, to avoid joining itself.
    std::jthread canceller([&source]
                          {
                              std::this_thread::sleep_for(test_cancel_timeout);
                              source.request_stop();
                          });
    sync_wait(pending);
    canceller.join();

    std::cout << "TestRuntimeTimerCancellationTask completed successfully." << std::endl;
}

TEST_CASE(TestTimeoutRunsOnceAndReleasesCallback)
{
    auto calls = std::make_shared<std::atomic<int>>(0);
    std::weak_ptr<int> lifetime;
    {
        runtime_context context;
        auto owner = std::make_shared<int>(42);
        lifetime = owner;
        auto finished = std::make_shared<event_signal>();
        const auto started = std::chrono::steady_clock::now();
        auto elapsed = std::make_shared<std::chrono::steady_clock::duration>();
        set_timeout([owner, calls, finished, elapsed, started]
                    {
                        *elapsed = std::chrono::steady_clock::now() - started;
                        ++*calls;
                        finished->set();
                    }, 20ms);
        owner.reset();
        finished->wait();
        EXPECT_GE(*elapsed, 19ms) << "Timeout callback ran before its deadline";
        sync_wait(sleep_for(20ms));
        EXPECT_EQ(calls->load(), 1) << "Timeout should fire once; elapsed ms="
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    }
    EXPECT_EQ(calls->load(), 1);
    EXPECT_TRUE(lifetime.expired()) << "Detached timeout frame must release its callback";
}

TEST_CASE(TestTimeoutCancellationReleasesCallback)
{
    auto calls = std::make_shared<std::atomic<int>>(0);
    std::weak_ptr<int> lifetime;
    {
        runtime_context context;
        std::stop_source stop;
        auto owner = std::make_shared<int>(1);
        lifetime = owner;
        set_timeout([owner, calls] { ++*calls; }, 1h, stop.get_token());
        owner.reset();
        EXPECT_FALSE(lifetime.expired());
        sync_wait(yield()); // Ensure native submission precedes cancellation.
        stop.request_stop();
    }
    EXPECT_EQ(calls->load(), 0);
    EXPECT_TRUE(lifetime.expired());
}

TEST_CASE(TestPreCancelledTimersAndImmediateTimeout)
{
    runtime_context context;
    std::stop_source stop;
    stop.request_stop();
    int calls = 0;
    set_timeout([&] { ++calls; }, 0ms, stop.get_token());
    set_interval([&] { ++calls; }, 10ms, stop.get_token());
    EXPECT_EQ(calls, 0);
    set_timeout([&] { ++calls; }, 0ms);
    EXPECT_EQ(calls, 1) << "Zero-duration timeout executes eagerly";
    set_timeout([&] { ++calls; }, -1ms);
    EXPECT_EQ(calls, 2);
}

TEST_CASE(TestIntervalRepeatsAndStopsFromCallback)
{
    auto calls = std::make_shared<std::atomic<int>>(0);
    std::weak_ptr<int> lifetime;
    {
        runtime_context context;
        std::stop_source stop;
        auto owner = std::make_shared<int>(1);
        lifetime = owner;
        auto finished = std::make_shared<event_signal>();
        set_interval([owner, calls, stop, finished]() mutable
                     {
                         if (++*calls == 3)
                         {
                             stop.request_stop();
                             finished->set();
                         }
                     }, 10ms, stop.get_token());
        owner.reset();
        finished->wait();
        sync_wait(sleep_for(20ms));
        EXPECT_EQ(calls->load(), 3) << "Interval should stop after its third callback";
        stop.request_stop();
    }
    EXPECT_EQ(calls->load(), 3);
    EXPECT_TRUE(lifetime.expired());
}

TEST_CASE(TestIntervalCancelledBeforeFirstCallback)
{
    auto calls = std::make_shared<std::atomic<int>>(0);
    {
        runtime_context context;
        std::stop_source stop;
        set_interval([calls] { ++*calls; }, 1h, stop.get_token());
        sync_wait(yield());
        stop.request_stop();
    }
    EXPECT_EQ(calls->load(), 0);
}
