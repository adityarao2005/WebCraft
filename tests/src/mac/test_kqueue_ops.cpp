///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////


#ifdef __APPLE__

#define TEST_SUITE_NAME KqueueTestSuite
#include "test_suite.hpp"
#include <sys/event.h>
#include <system_error>
#include <cerrno>
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std::chrono_literals;

TEST_CASE(SampleTest)
{
    EXPECT_EQ(1 + 1, 2);
}

int initialize_kqueue()
{
    // create the queue
    int queue = kqueue();
    if (queue == -1)
        throw std::system_error(errno, std::generic_category(), "kqueue");
    TEST_TRACE("kqueue initialized");

    return queue;
}

void destroy_kqueue(int queue)
{
    // close the queue
    int result = close(queue);
    EXPECT_GE(result, 0) << "Failed to close kqueue:" << result;
}

// Test case to initialize and clean up kqueue
TEST_CASE(kqueue_setup_and_cleanup)
{
    int queue = initialize_kqueue();

    // Cleanup
    destroy_kqueue(queue);
}

void post_nop_event(int queue, uint64_t payload)
{
    uint64_t id = generate_random_uint64();

    // listen to the yield event
    struct kevent event;
    EV_SET(&event, id, EVFILT_USER, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    int result = kevent(queue, &event, 1, nullptr, 0, nullptr);
    EXPECT_EQ(result, 0) << "Failed to register event to kqueue:" << result;

    EV_SET(&event, id, EVFILT_USER, 0, NOTE_TRIGGER, 0, new uint64_t(payload));
    result = kevent(queue, &event, 1, nullptr, 0, nullptr);
    EXPECT_GE(result, 0) << "Failed to trigger event in kqueue:" << result;
}

auto wait_and_get_event(int queue)
{
    struct kevent event;
    TEST_TRACE("waiting for kqueue completion");
    int result = kevent(queue, nullptr, 0, &event, 1, nullptr);
    if (result < 0)
        throw std::system_error(errno, std::generic_category(), "kevent wait");

    auto *ptr_data = reinterpret_cast<uint64_t *>(event.udata);
    auto data = *ptr_data;
    delete ptr_data;

    // remove yield event listener
    EV_SET(&event, event.ident, event.filter, EV_DELETE, 0, 0, nullptr);
    result = kevent(queue, &event, 1, nullptr, 0, nullptr);
    EXPECT_GE(result, 0) << "Failed to remove event to kqueue:" << result;

    return data;
}

void wait_for_event(int queue, uint64_t payload)
{
    auto user_data = wait_and_get_event(queue);

    EXPECT_EQ(user_data, payload) << "Payload mismatch in completion event. Expected: " << payload << ", Got: " << user_data;
}

// Test cases for posting and waiting for events with no payload
TEST_CASE(kqueue_post_and_wait_for_event_no_payload)
{
    int queue = initialize_kqueue();

    // Post an event
    post_nop_event(queue, 0);

    // Wait for the event to complete
    wait_for_event(queue, 0);

    // Cleanup
    destroy_kqueue(queue);
}

// Test case for posting and waiting for an event with a specific payload
TEST_CASE(kqueue_post_and_wait_for_event_random_payload)
{
    int queue = initialize_kqueue();

    uint64_t random_payload = generate_random_uint64();

    // Post an event
    post_nop_event(queue, random_payload);

    // Wait for the event to complete
    wait_for_event(queue, random_payload);

    // Cleanup
    destroy_kqueue(queue);
}

// Test case for posting a callback and executing it when the event is completed
TEST_CASE(kqueue_post_and_test_callback)
{
    int queue = initialize_kqueue();

    webcraft::async::event_signal callback_called;

    struct callback_event
    {
        webcraft::async::event_signal *callback_flag;

        void operator()()
        {
            callback_flag->set(); // Set the callback flag to true
        }
    };

    callback_event cb_event{&callback_called};

    post_nop_event(queue, reinterpret_cast<uint64_t>(&cb_event));

    callback_event *ptr = reinterpret_cast<callback_event *>(wait_and_get_event(queue));

    (*ptr)(); // Call the callback

    EXPECT_TRUE(callback_called.is_set()) << "Callback was not called";

    // Cleanup
    destroy_kqueue(queue);
}

// Test case for waiting for multiple events
TEST_CASE(kqueue_wait_multiple_events)
{
    // Define the number of events to wait for
    constexpr int num_events = 5;

    // initialize the structures
    webcraft::async::event_signal signals[num_events];
    int queue = initialize_kqueue();

    // the callback event structure
    struct callback_event
    {
        webcraft::async::event_signal *callback_flag;

        void operator()()
        {
            callback_flag->set(); // Set the callback flag to true
        }
    };

    // spawn and post multiple callback events
    for (int i = 0; i < num_events; ++i)
    {
        callback_event *cb_event = new callback_event{&signals[i]};
        post_nop_event(queue, reinterpret_cast<uint64_t>(cb_event));
    }

    // Wait for all events to complete and call the callbacks
    for (int i = 0; i < num_events; ++i)
    {
        callback_event *ptr = reinterpret_cast<callback_event *>(wait_and_get_event(queue));
        (*ptr)();   // Call the callback
        delete ptr; // Clean up the callback event
    }

    // Check if all signals were set
    for (int i = 0; i < num_events; ++i)
    {
        EXPECT_TRUE(signals[i].is_set()) << "Callback " << i << " was not called";
    }

    // Cleanup
    destroy_kqueue(queue);
}

void post_timer_event(int queue, std::chrono::steady_clock::duration duration, uint64_t payload)
{
    uint64_t id = generate_random_uint64();
    struct kevent event;
    EV_SET(&event, id, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_NSECONDS, std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(), new uint64_t(payload));
    int result = kevent(queue, &event, 1, nullptr, 0, nullptr);
    EXPECT_GE(result, 0) << "Failed to spawn timer event to kqueue:" << result;
}

void wait_for_timeout_event(int queue, uint64_t payload)
{
    wait_for_event(queue, payload);
}

TEST_CASE(kqueue_test_timer)
{
    constexpr int payload = 0;

    int queue = initialize_kqueue();

    post_timer_event(queue, test_timer_timeout, payload);

    auto start = std::chrono::steady_clock::now();
    wait_for_timeout_event(queue, payload);
    auto end = std::chrono::steady_clock::now();

    auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GE(elapsed_time, test_timer_timeout) << "Timer event did not complete after the expected duration";

    destroy_kqueue(queue);
}

#endif