///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////


#ifdef _WIN32

#define TEST_SUITE_NAME IOCPTestSuite
#include "test_suite.hpp"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <system_error>
#include <WinSock2.h>
#include <webcraft/async/runtime/windows/windows_timer_manager.hpp>
#include <webcraft/async/runtime/windows.event.hpp>

using namespace std::chrono_literals;

TEST_CASE(SampleTest)
{
    EXPECT_EQ(1 + 1, 2);
}

HANDLE initialize_iocp()
{
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    EXPECT_EQ(result, 0) << "Failed to initialize Winsock: " << result;

    // Create an IO Completion Port (IOCP)
    auto iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    HANDLE null_handle = NULL;
    if (iocp == null_handle)
        throw std::system_error(GetLastError(), std::system_category(), "CreateIoCompletionPort");
    TEST_TRACE("IOCP initialized");
    return iocp;
}

void destroy_iocp(HANDLE iocp)
{
    auto check = CloseHandle(iocp);
    EXPECT_TRUE(check) << "Failed to close IOCP handle";

    // Cleanup Winsock
    int result = WSACleanup();
    EXPECT_EQ(result, 0) << "Failed to cleanup Winsock: " << result;
}

// Test case to initialize and clean up IOCP
TEST_CASE(iocp_setup_and_cleanup)
{
    HANDLE iocp = initialize_iocp();

    // Cleanup
    destroy_iocp(iocp);
}

struct overlapped_event : public OVERLAPPED
{
    uint64_t payload;
};

void post_nop_event(HANDLE iocp, uint64_t payload)
{
    // Post a NOP operation to the IOCP
    overlapped_event *overlapped = new overlapped_event();
    memset(overlapped, 0, sizeof(OVERLAPPED));
    overlapped->payload = payload;

    BOOL result = PostQueuedCompletionStatus(iocp, 0, payload, overlapped);
    EXPECT_TRUE(result) << "Failed to post NOP event to IOCP";
}

auto wait_and_get_event(HANDLE iocp)
{
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    OVERLAPPED *overlapped = nullptr;

    TEST_TRACE("waiting for IOCP completion");
    BOOL result = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &overlapped, INFINITE);
    if (!result)
        throw std::system_error(GetLastError(), std::system_category(), "GetQueuedCompletionStatus");

    // Process the completion event
    EXPECT_NE(overlapped, nullptr) << "Completion event is null";
    auto event = static_cast<overlapped_event *>(overlapped);
    uint64_t user_data = event->payload;

    delete event; // Clean up the overlapped structure
    return user_data;
}

void wait_for_event(HANDLE iocp, uint64_t expected_payload)
{
    auto payload = wait_and_get_event(iocp);
    EXPECT_EQ(payload, expected_payload) << "Payload mismatch in completion event. Expected: " << expected_payload << ", Got: " << payload;
}

// Test cases for posting and waiting for events with no payload
TEST_CASE(iocp_post_and_wait_for_event_no_payload)
{
    HANDLE iocp = initialize_iocp();

    // Post an event
    post_nop_event(iocp, 0);

    // Wait for the event to complete
    wait_for_event(iocp, 0);

    // Cleanup
    destroy_iocp(iocp);
}

// Test case for posting and waiting for an event with a specific payload
TEST_CASE(iocp_post_and_wait_for_event_random_payload)
{
    HANDLE iocp = initialize_iocp();

    uint64_t random_payload = generate_random_uint64();

    // Post an event
    post_nop_event(iocp, random_payload);

    // Wait for the event to complete
    wait_for_event(iocp, random_payload);

    // Cleanup
    destroy_iocp(iocp);
}

// Test case for posting a callback and executing it when the event is completed
TEST_CASE(iocp_post_and_test_callback)
{
    HANDLE iocp = initialize_iocp();

    // Create a signal to test the callback
    webcraft::async::event_signal signal;

    struct callback_event
    {
        webcraft::async::event_signal *signal;

        void operator()()
        {
            signal->set(); // Set the signal to indicate the callback was called
        }
    };

    callback_event cb_event{&signal};

    // Post an event with a callback
    post_nop_event(iocp, reinterpret_cast<uint64_t>(&cb_event));

    // Wait for the event to complete and call the callback
    callback_event *ptr = reinterpret_cast<callback_event *>(wait_and_get_event(iocp));

    (*ptr)(); // Call the callback

    // Check if the signal was set
    EXPECT_TRUE(signal.is_set()) << "Callback was not called";

    // Cleanup
    destroy_iocp(iocp);
}

// Test case for waiting for multiple events
TEST_CASE(iocp_wait_multiple_events)
{
    // Define the number of events to wait for
    constexpr int num_events = 5;

    // Initialize the IOCP
    HANDLE iocp = initialize_iocp();

    // Create an array of signals
    webcraft::async::event_signal signals[num_events];

    // the callback event structure
    struct callback_event
    {
        webcraft::async::event_signal *signal;

        void operator()()
        {
            signal->set(); // Set the signal to indicate the callback was called
        }
    };

    // Post multiple callback events
    for (int i = 0; i < num_events; ++i)
    {
        callback_event *cb_event = new callback_event{&signals[i]};
        post_nop_event(iocp, reinterpret_cast<uint64_t>(cb_event));
    }

    // Wait for all events to complete and call the callbacks
    for (int i = 0; i < num_events; ++i)
    {
        callback_event *ptr = reinterpret_cast<callback_event *>(wait_and_get_event(iocp));
        (*ptr)();   // Call the callback
        delete ptr; // Clean up the callback event
    }

    // Check if all signals were set
    for (int i = 0; i < num_events; ++i)
    {
        EXPECT_TRUE(signals[i].is_set()) << "Callback " << i << " was not called";
    }

    // Cleanup
    destroy_iocp(iocp);
}

PTP_TIMER post_timer_event(HANDLE iocp, webcraft::async::runtime::detail::timer_manager &context, std::chrono::steady_clock::duration duration, uint64_t payload)
{
    return context.post_timer_event(duration, [iocp, payload]()
                                    {
        // This callback will be called when the timer expires
        post_nop_event(iocp, payload); });
}

void wait_for_timeout_event(HANDLE iocp, uint64_t payload)
{
    std::cout << "Waiting for timer event with payload: " << payload << std::endl;
    wait_for_event(iocp, payload);
    std::cout << "Timer event completed with payload: " << payload << std::endl;
}

TEST_CASE(iocp_test_timer)
{
    constexpr int payload = 0;
    std::cout << "Testing IOCP timer functionality..." << std::endl;

    webcraft::async::runtime::detail::timer_manager manager;
    // Initialize the IOCP
    HANDLE iocp = initialize_iocp();

    std::cout << "Posting timer event with sleep time: " << test_timer_timeout.count() << " seconds" << std::endl;

    post_timer_event(iocp, manager, test_timer_timeout, payload);

    auto start = std::chrono::steady_clock::now();
    wait_for_timeout_event(iocp, payload);
    auto end = std::chrono::steady_clock::now();

    auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start + test_adjustment_factor);

    EXPECT_GE(elapsed_time, test_timer_timeout) << "Timer event did not complete after the expected duration";

    destroy_iocp(iocp);
}

TEST_CASE(try_cancellation_test)
{
    HANDLE iocp = initialize_iocp();

    webcraft::async::runtime::detail::timer_manager manager;

    PTP_TIMER timer = post_timer_event(iocp, manager, test_timer_timeout, 0);

    std::jthread cancel_thread([&manager, &timer, iocp]()
                               {
                                   std::this_thread::sleep_for(test_cancel_timeout);
                                   manager.cancel_timer(timer); // Cancel the timer after 2 seconds
                                   post_nop_event(iocp, 1);     // Post a dummy event to signal cancellation
                               });

    // Wait for the event to complete
    auto payload = wait_and_get_event(iocp);

    EXPECT_EQ(payload, 1) << "Expected cancellation event payload to be 1, but got: " << payload;

    destroy_iocp(iocp);
}

TEST_CASE(try_cancellation_test_with_stop_token)
{
    HANDLE iocp = initialize_iocp();

    webcraft::async::runtime::detail::timer_manager manager;

    std::stop_source source;
    PTP_TIMER timer = post_timer_event(iocp, manager, test_timer_timeout, 0);

    std::jthread cancel_thread([&manager, &source]()
                               {
                                   std::this_thread::sleep_for(test_cancel_timeout);
                                   source.request_stop(); // Request cancellation after 2 seconds
                               });

    std::stop_token token = source.get_token();
    std::stop_callback cancel_callback(token, [&manager, &timer, iocp]()
                                       {
                                           manager.cancel_timer(timer); // Cancel the timer after 2 seconds
                                           post_nop_event(iocp, 1);     // Post a dummy event to signal cancellation
                                       });

    // Wait for the event to complete
    auto payload = wait_and_get_event(iocp);

    EXPECT_EQ(payload, 1) << "Expected cancellation event payload to be 1, but got: " << payload;

    destroy_iocp(iocp);
}

TEST_CASE(try_cancellation_test_with_stop_token_and_callback)
{
    HANDLE iocp = initialize_iocp();

    ::webcraft::async::event_signal canceled_signal, resume_signal;

    struct enable_signal
    {
        webcraft::async::event_signal *signal;

        void operator()()
        {
            signal->set(); // Set the signal to indicate the callback was called
        }
    };

    webcraft::async::runtime::detail::timer_manager manager;

    std::stop_source source;
    PTP_TIMER timer = manager.post_timer_event(test_timer_timeout, [&resume_signal, iocp]()
                                               {
                                                   resume_signal.set();                                                                 // Set the resume signal when the timer expires
                                                   post_nop_event(iocp, reinterpret_cast<uint64_t>(new enable_signal(&resume_signal))); // Post a dummy event to signal completion
                                               });

    std::stop_token token = source.get_token();
    std::stop_callback cancel_callback(token, [&manager, &timer, iocp, &canceled_signal]()
                                       {
                                           manager.cancel_timer(timer);                                                           // Cancel the timer after 2 seconds
                                           post_nop_event(iocp, reinterpret_cast<uint64_t>(new enable_signal(&canceled_signal))); // Post a dummy event to signal cancellation
                                       });
    std::jthread cancel_thread([&manager, &source]()
                               {
                                   std::this_thread::sleep_for(test_cancel_timeout);
                                   source.request_stop(); // Request cancellation after some time
                               });

    // Wait for the event to complete
    auto start = std::chrono::steady_clock::now();
    auto payload = wait_and_get_event(iocp);
    auto end = std::chrono::steady_clock::now();
    auto signal = reinterpret_cast<enable_signal *>(payload);
    (*signal)();   // Call the callback to set the signal
    delete signal; // Clean up the callback event

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_TRUE(canceled_signal.is_set()) << "Expected cancellation signal to be set, but it was not";
    EXPECT_GE(duration, test_cancel_timeout)
        << "Expected cancellation to occur after the cancel time, but it did not";
    EXPECT_LE(duration, test_timer_timeout)
        << "Expected cancellation to occur before the sleep time, but it did not";

    std::this_thread::sleep_for(test_timer_timeout);

    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration, test_timer_timeout + test_adjustment_factor) << "Expected resume signal to be set after the sleep time, but it was not";

    EXPECT_FALSE(resume_signal.is_set()) << "Expected resume signal to not be set, but it was";

    destroy_iocp(iocp);
}

TEST_CASE(NativeIoCompletionWaitsForPacket)
{
    using namespace webcraft::async::detail::windows;
    for (bool immediate_success : {true, false})
    {
        SCOPED_TRACE(immediate_success ? "synchronous native success" : "pending native success");
        std::unique_ptr<void, decltype(&destroy_iocp)> port(initialize_iocp(), &destroy_iocp);
        OVERLAPPED *submitted = nullptr;
        int completions = 0;
        // Queue a packet but do not dispatch it until start_async() has returned.
        // This models both native success paths without relying on disk timing.
        std::unique_ptr<webcraft::async::detail::runtime_event> event = create_async_io_overlapped_event(
            INVALID_HANDLE_VALUE,
            [&](LPDWORD bytes, LPOVERLAPPED ptr) -> BOOL
            {
                submitted = ptr;
                *bytes = 17;
                if (!PostQueuedCompletionStatus(port.get(), *bytes, 0, ptr))
                    throw overlapped_runtime_event_error("Failed to queue test completion");
                if (immediate_success)
                    return TRUE;
                SetLastError(ERROR_IO_PENDING);
                return FALSE;
            }, std::stop_token{});
        event->start_async([&]
                           {
                               ++completions;
                               event.reset();
                           });
        EXPECT_EQ(completions, 0) << "Native success must await its queued packet";
        EXPECT_NE(event, nullptr) << "OVERLAPPED storage must survive until dequeue";

        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED *packet = nullptr;
        BOOL received = GetQueuedCompletionStatus(port.get(), &bytes, &key, &packet, 0);
        DWORD receive_error = received ? ERROR_SUCCESS : GetLastError();
        EXPECT_TRUE(received) << "GetQueuedCompletionStatus failed: " << receive_error;
        EXPECT_EQ(packet, submitted);
        EXPECT_EQ(bytes, 17);
        if (received && packet && event)
        {
            auto *completion = static_cast<webcraft::async::detail::windows::overlapped_event *>(packet);
            completion->event->try_execute(static_cast<int>(bytes));
        }
        EXPECT_EQ(completions, 1);
        EXPECT_EQ(event, nullptr) << "The completion callback must safely release the event";

        BOOL extra_packet = GetQueuedCompletionStatus(port.get(), &bytes, &key, &packet, 0);
        DWORD extra_error = GetLastError();
        EXPECT_FALSE(extra_packet);
        EXPECT_EQ(extra_error, WAIT_TIMEOUT) << "Exactly one packet should be posted";
    }
}

TEST_CASE(ExplicitInlineOverlappedCompletionRemainsInline)
{
    int completions = 0;
    auto event = webcraft::async::detail::windows::create_overlapped_event(
        [](LPDWORD bytes, LPOVERLAPPED) -> BOOL
        {
            *bytes = 17;
            return TRUE; // No native operation and no queued packet.
        }, [](LPOVERLAPPED) {}, std::stop_token{});
    event->start_async([&] { ++completions; });
    EXPECT_EQ(completions, 1);
    EXPECT_EQ(event->get_result(), 17);
}

#endif
