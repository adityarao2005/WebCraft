///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#ifdef __linux__

#define TEST_SUITE_NAME LinuxTcpSendTestSuite

#include "test_suite.hpp"
#include "../../../src/webcraft/async_tcp_socket_decl.hpp"
#include <array>
#include <cerrno>
#include <csignal>
#include <system_error>
#include <sys/socket.h>
#include <unistd.h>

using namespace webcraft::async;

// Local stream pairs isolate send/error semantics without fixed-port listeners.
// TCP loopback behavior is also covered by the shared async socket suite.
TEST_CASE(TestConnectedStreamSend)
{
    runtime_context context;
    int sockets[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), 0);
    io_uring_tcp_socket_descriptor sender(sockets[0], "local", 0);
    const std::string message = "socket send payload";
    auto written = sync_wait(sender.write(std::span<const char>(message)));
    std::array<char, 64> received{};
    auto count = ::recv(sockets[1], received.data(), received.size(), MSG_DONTWAIT);
    ::close(sockets[1]);
    sync_wait(sender.close());
    EXPECT_EQ(written, message.size());
    EXPECT_EQ(count, static_cast<ssize_t>(message.size()));
    if (count > 0)
    {
        EXPECT_EQ(std::string(received.data(), static_cast<std::size_t>(count)), message);
    }
}

TEST_CASE(TestEmptyStreamSend)
{
    runtime_context context;
    int sockets[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), 0);
    io_uring_tcp_socket_descriptor sender(sockets[0], "local", 0);
    auto written = sync_wait(sender.write(std::span<const char>{}));
    ::close(sockets[1]);
    sync_wait(sender.close());
    EXPECT_EQ(written, 0u);
}

namespace
{
    volatile std::sig_atomic_t received_sigpipe = 0;

    void record_sigpipe(int)
    {
        received_sigpipe = 1;
    }
}

TEST_CASE(TestClosedPeerReportsErrorWithoutSigpipe)
{
    runtime_context context;
    int sockets[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), 0);
    io_uring_tcp_socket_descriptor sender(sockets[0], "local", 0);
    ::close(sockets[1]);
    received_sigpipe = 0;
    auto previous = std::signal(SIGPIPE, record_sigpipe);
    std::error_code error;
    const std::string message = "closed peer";
    try
    {
        sync_wait(sender.write(std::span<const char>(message)));
    }
    catch (const std::system_error &failure)
    {
        error = failure.code();
    }
    sync_wait(sender.close());
    std::signal(SIGPIPE, previous);
    EXPECT_EQ(error.value(), EPIPE);
    EXPECT_EQ(received_sigpipe, 0);
}

#endif
