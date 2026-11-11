///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////


#ifdef __linux__

#define TEST_SUITE_NAME IOUringTestSuite
#include "test_suite.hpp"
#include <liburing.h>
#include <system_error>

using namespace std::chrono_literals;

TEST_CASE(SampleTest)
{
    EXPECT_EQ(1 + 1, 2);
}

void initialize_io_uring(struct io_uring *ring)
{
    int ret = io_uring_queue_init(8, ring, 0);
    if (ret < 0)
        throw std::system_error(-ret, std::generic_category(), "io_uring_queue_init");
    TEST_TRACE("io_uring initialized");
}

void destroy_io_uring(struct io_uring *ring)
{
    io_uring_queue_exit(ring);
}

// Test case to initialize and clean up io_uring
TEST_CASE(io_uring_setup_and_cleanup)
{
    struct io_uring ring;
    initialize_io_uring(&ring);

    // Cleanup
    destroy_io_uring(&ring);
}

void post_nop_event(struct io_uring *ring, uint64_t payload)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe)
        throw std::runtime_error("io_uring_get_sqe: submission queue is full");

    // Prepare a NOP operation to signal the completion of the task
    io_uring_prep_nop(sqe);
    io_uring_sqe_set_data64(sqe, payload);
    int ret = io_uring_submit(ring);
    if (ret < 0)
        throw std::system_error(-ret, std::generic_category(), "io_uring_submit");
}

auto wait_and_get_event(struct io_uring *ring)
{
    struct io_uring_cqe *cqe = nullptr;
    TEST_TRACE("waiting for io_uring completion");
    int ret = io_uring_wait_cqe(ring, &cqe);
    if (ret < 0)
        throw std::system_error(-ret, std::generic_category(), "io_uring_wait_cqe");

    // Process the completion event
    EXPECT_NE(cqe, nullptr) << "Completion event is null";
    auto user_data = cqe->user_data;
    io_uring_cqe_seen(ring, cqe);
    return user_data;
}

void wait_for_event(struct io_uring *ring, uint64_t payload)
{
    auto user_data = wait_and_get_event(ring);

    EXPECT_EQ(user_data, payload) << "Payload mismatch in completion event. Expected: " << payload << ", Got: " << user_data;
}

// Test cases for posting and waiting for events with no payload
TEST_CASE(io_uring_post_and_wait_for_event_no_payload)
{
    struct io_uring ring;
    initialize_io_uring(&ring);

    // Post an event
    post_nop_event(&ring, 0);

    // Wait for the event to complete
    wait_for_event(&ring, 0);

    // Cleanup
    destroy_io_uring(&ring);
}

// Test case for posting and waiting for an event with a specific payload
TEST_CASE(io_uring_post_and_wait_for_event_random_payload)
{
    struct io_uring ring;
    initialize_io_uring(&ring);

    uint64_t random_payload = generate_random_uint64();

    // Post an event
    post_nop_event(&ring, random_payload);

    // Wait for the event to complete
    wait_for_event(&ring, random_payload);

    // Cleanup
    destroy_io_uring(&ring);
}

// Test case for posting a callback and executing it when the event is completed
TEST_CASE(io_uring_post_and_test_callback)
{
    struct io_uring ring;
    initialize_io_uring(&ring);

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

    post_nop_event(&ring, reinterpret_cast<uint64_t>(&cb_event));

    callback_event *ptr = reinterpret_cast<callback_event *>(wait_and_get_event(&ring));

    (*ptr)(); // Call the callback

    EXPECT_TRUE(callback_called.is_set()) << "Callback was not called";

    // Cleanup
    destroy_io_uring(&ring);
}

// Test case for waiting for multiple events
TEST_CASE(io_uring_wait_multiple_events)
{
    // Define the number of events to wait for
    constexpr int num_events = 5;

    // initialize the structures
    webcraft::async::event_signal signals[num_events];
    struct io_uring ring;
    initialize_io_uring(&ring);

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
        post_nop_event(&ring, reinterpret_cast<uint64_t>(cb_event));
    }

    // Wait for all events to complete and call the callbacks
    for (int i = 0; i < num_events; ++i)
    {
        callback_event *ptr = reinterpret_cast<callback_event *>(wait_and_get_event(&ring));
        (*ptr)();   // Call the callback
        delete ptr; // Clean up the callback event
    }

    // Check if all signals were set
    for (int i = 0; i < num_events; ++i)
    {
        EXPECT_TRUE(signals[i].is_set()) << "Callback " << i << " was not called";
    }

    // Cleanup
    destroy_io_uring(&ring);
}

void post_timer_event(struct io_uring *ring, std::chrono::steady_clock::duration duration, uint64_t payload)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe)
        throw std::runtime_error("io_uring_get_sqe: submission queue is full");

    // Prepare a timer event

    struct __kernel_timespec its;
    its.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    its.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration % std::chrono::seconds(1)).count();

    io_uring_prep_timeout(sqe, &its, 0, 0);
    io_uring_sqe_set_data64(sqe, payload);
    int ret = io_uring_submit(ring);
    if (ret < 0)
        throw std::system_error(-ret, std::generic_category(), "io_uring_submit");
}

void wait_for_timeout_event(struct io_uring *ring, uint64_t payload)
{
    struct io_uring_cqe *cqe = nullptr;
    TEST_TRACE("waiting for io_uring completion");
    int ret = io_uring_wait_cqe(ring, &cqe);
    if (ret < 0)
        throw std::system_error(-ret, std::generic_category(), "io_uring_wait_cqe");

    // Process the completion event
    EXPECT_NE(cqe, nullptr) << "Completion event is null";
    EXPECT_GE(cqe->res, -ETIME) << "Completion event result is negative: " << cqe->res;

    EXPECT_EQ(cqe->user_data, payload) << "Payload mismatch in completion event. Expected: " << payload << ", Got: " << cqe->user_data;

    io_uring_cqe_seen(ring, cqe);
}

TEST_CASE(io_uring_test_timer)
{
    constexpr int payload = 0;

    struct io_uring ring;
    initialize_io_uring(&ring);

    post_timer_event(&ring, test_timer_timeout, payload);

    auto start = std::chrono::steady_clock::now();
    wait_for_timeout_event(&ring, payload);
    auto end = std::chrono::steady_clock::now();

    auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GE(elapsed_time, test_timer_timeout) << "Timer event did not complete after the expected duration";

    destroy_io_uring(&ring);
}

#endif