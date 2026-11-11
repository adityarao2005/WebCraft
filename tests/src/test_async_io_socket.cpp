///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#define TEST_SUITE_NAME AsyncSocketTestSuite

#include "test_suite.hpp"
#include <webcraft/async/async.hpp>
#include "mock_io.hpp"

using namespace webcraft::async;
using namespace webcraft::async::io::socket;

const struct connection_info info = {"127.0.0.1", 12345};
const ip_version version = ip_version::IPv4;

TEST_CASE(TestMockUdpWorks)
{
    runtime_context context;
    std::cout << "Starting UDP Echo Server on " << info.host << ":" << info.port << std::endl;
    webcraft::test::udp::echo_server server(info);
    std::cout << "Creating UDP Echo Client" << std::endl;
    webcraft::test::udp::echo_client client(version);

    const std::string message = "Hello, UDP Echo!";

    for (size_t i = 0; i < 5; ++i)
    {
        std::cout << "UDP Echo Attempt " << (i + 1) << std::endl;
        bool success = client.echo(message, info);
        EXPECT_TRUE(success) << "UDP Echo should succeeded on the " << (i + 1) << " attempt";
    }

    std::cout << "Closing UDP Echo Client and Server" << std::endl;
    client.close();
    std::cout << "Client closed" << std::endl;
    server.close();
}

TEST_CASE(TestMockTcpWorks)
{
    runtime_context context;
    std::cout << "Starting TCP Echo Server on " << info.host << ":" << info.port << std::endl;
    webcraft::test::tcp::echo_server server(info);
    std::cout << "Creating TCP Echo Client for server at " << info.host << ":" << info.port << std::endl;
    webcraft::test::tcp::echo_client client(info);

    const std::string message = "Hello, TCP Echo!";

    for (size_t i = 0; i < 5; ++i)
    {
        std::cout << "TCP Echo Attempt " << (i + 1) << std::endl;
        bool success = client.echo(message);
        EXPECT_TRUE(success) << "TCP Echo should succeeded on the " << (i + 1) << " attempt";
    }

    std::cout << "Closing TCP Echo Client and Server" << std::endl;
    client.close();
    std::cout << "Client closed" << std::endl;
    server.close();
}

class async_tcp_echo_client
{
private:
    tcp_socket socket;

public:
    async_tcp_echo_client() : socket(make_tcp_socket()) {}

    task<void> close()
    {
        co_await socket.close();
    }

    task<void> connect(const connection_info &info)
    {
        co_await socket.connect(info);
    }

    task<bool> echo(const std::string &message)
    {
        auto &writer = socket.get_writable_stream();
        auto &reader = socket.get_readable_stream();

        std::cout << "### Client > Sending data: " << message << std::endl;
        size_t bytes_sent = co_await writer.send(std::span<const char>(message.data(), message.size()));
        if (bytes_sent != message.size())
        {
            co_return false; // Error sending data
        }
        std::cout << "### Client > Data sent properly: " << bytes_sent << " bytes" << ", content being: " << message << std::endl;

        std::cout << "### Client > Receiving data" << std::endl;
        std::vector<char> buffer(message.size());
        size_t bytes_received = co_await reader.recv(std::span<char>(buffer.data(), buffer.size()));
        if (bytes_received != message.size())
        {
            co_return false; // Error receiving data
        }
        std::cout << "### Client > Data received properly: " << bytes_received << ", content being: " << std::string(buffer.data(), bytes_received) << std::endl;

        co_return std::string(buffer.data(), buffer.size()) == message; // Check if the received message matches the sent message
    }
};

TEST_CASE(TestAsyncTcpSocket)
{
    runtime_context context;
    std::cout << "Starting TCP Echo Server on " << info.host << ":" << info.port << std::endl;
    webcraft::test::tcp::echo_server server(info);
    std::cout << "Creating Async TCP Socket for server at " << info.host << ":" << info.port << std::endl;

    auto task_fn = co_async
    {
        async_tcp_echo_client client;
        co_await client.connect(info);
        const std::string message = "Hello, Async TCP Echo!";

        for (size_t i = 0; i < 5; ++i)
        {
            std::cout << "Async TCP Echo Attempt " << (i + 1) << std::endl;
            bool success = co_await client.echo(message);
            EXPECT_TRUE(success) << "Async TCP Echo should succeeded on the " << (i + 1) << " attempt";
        }

        std::cout << "Closing Async TCP Echo Client" << std::endl;
        co_await client.close();
    };

    sync_wait(task_fn());

    std::cout << "Closing TCP Echo Server" << std::endl;
    server.close();
}

class async_tcp_echo_server
{
private:
    tcp_listener listener;
    std::stop_source source;
    const connection_info &info;

public:
    async_tcp_echo_server(const connection_info &info) : listener(make_tcp_listener()), info(info) {}
    ~async_tcp_echo_server()
    {
        sync_wait(shutdown());
    }

    task<void> shutdown()
    {
        if (!source.stop_requested())
        {
            source.request_stop();

            tcp_socket dummy_socket = make_tcp_socket();
            co_await dummy_socket.connect(info); // Connect to unblock accept
            co_await dummy_socket.close();

            co_await listener.close();
        }
        co_return;
    }

    task<void> run(const connection_info &info)
    {
        std::stop_token token = source.get_token();
        listener.bind(info);
        listener.listen(5);
        while (!token.stop_requested())
        {
            tcp_socket client_socket = co_await listener.accept();

            handle_client(std::move(client_socket));
        }
    }

    fire_and_forget_task handle_client(tcp_socket client_socket)
    {
        std::stop_token token = source.get_token();
        auto &reader = client_socket.get_readable_stream();
        auto &writer = client_socket.get_writable_stream();
        std::vector<char> buffer(1024);

        while (!token.stop_requested())
        {
            size_t bytes_received = co_await reader.recv(std::span<char>(buffer.data(), buffer.size()));
            if (bytes_received == 0)
            {
                break; // Connection closed by client
            }

            if (token.stop_requested())
            {
                break; // Connection closed by client
            }

            size_t bytes_sent = co_await writer.send(std::span<const char>(buffer.data(), bytes_received));
            if (bytes_sent != bytes_received)
            {
                break; // Error sending data
            }
        }

        co_await client_socket.close();
    }
};

TEST_CASE(TestAsyncTcpServer)
{
    runtime_context context;
    std::cout << "Starting Async TCP Echo Server on " << info.host << ":" << info.port << std::endl;
    async_tcp_echo_server server(info);
    auto server_task = server.run(info);

    std::cout << "Creating TCP Echo Client for server at " << info.host << ":" << info.port << std::endl;
    webcraft::test::tcp::echo_client client(info);

    const std::string message = "Hello, Async TCP Echo Server!";

    for (size_t i = 0; i < 5; ++i)
    {
        std::cout << "TCP Echo Attempt " << (i + 1) << std::endl;
        bool success = client.echo(message);
        EXPECT_TRUE(success) << "TCP Echo should succeeded on the " << (i + 1) << " attempt";
    }

    std::cout << "Closing TCP Echo Client" << std::endl;
    client.close();
    std::cout << "Shutting down Async TCP Echo Server" << std::endl;
    sync_wait(server.shutdown());
    sync_wait(server_task);
}

TEST_CASE(TestAsyncTcpServerWithAsyncClient)
{
    runtime_context context;
    std::cout << "Starting Async TCP Echo Server on " << info.host << ":" << info.port << std::endl;
    async_tcp_echo_server server(info);
    auto server_task = server.run(info);

    std::cout << "Creating Async TCP Socket for server at " << info.host << ":" << info.port << std::endl;

    auto task_fn = co_async
    {
        async_tcp_echo_client client;
        co_await client.connect(info);
        const std::string message = "Hello, Async TCP Echo Server!";

        for (size_t i = 0; i < 5; ++i)
        {
            std::cout << "Async TCP Echo Attempt " << (i + 1) << std::endl;
            bool success = co_await client.echo(message);
            EXPECT_TRUE(success) << "Async TCP Echo should succeeded on the " << (i + 1) << " attempt";
        }

        std::cout << "Closing Async TCP Echo Client" << std::endl;
        co_await client.close();
    };

    sync_wait(task_fn());

    std::cout << "Shutting down Async TCP Echo Server" << std::endl;
    sync_wait(server.shutdown());
    sync_wait(server_task);
}

class async_udp_echo_client
{
private:
    udp_socket socket;

public:
    async_udp_echo_client(std::optional<ip_version> ipv) : socket(make_udp_socket(ipv)) {}

    task<void> close()
    {
        co_await socket.close();
    }

    task<bool> echo(const std::string &message, const connection_info &server_info)
    {
        size_t bytes_sent = co_await socket.sendto(std::span<const char>(message.data(), message.size()), server_info);

        std::cout << "Sent " << bytes_sent << " bytes to " << server_info.host << ":" << server_info.port << std::endl;

        if (bytes_sent != message.size())
        {
            co_return false; // Error sending data
        }

        std::vector<char> buffer(1024);
        connection_info sender_info{};
        size_t bytes_received = co_await socket.recvfrom(std::span<char>(buffer.data(), buffer.size()), sender_info);
        std::cout << "Received " << bytes_received << " bytes from " << sender_info.host << ":" << sender_info.port << std::endl;
        if (bytes_received != message.size())
        {
            co_return false; // Error receiving data
        }

        if (sender_info.host != server_info.host || sender_info.port != server_info.port)
        {
            co_return false; // Received from unexpected sender
        }

        co_return std::string(buffer.data(), bytes_received) == message; // Check if the received message matches the sent message
    }
};

class async_udp_echo_server
{
private:
    udp_socket socket;
    std::stop_source source;

public:
    async_udp_echo_server(const connection_info &info) : socket(make_udp_socket())
    {
        socket.bind(info);
    }

    task<void> run()
    {
        while (!source.stop_requested())
        {
            std::vector<char> buffer(1024);
            connection_info sender_info;

            size_t bytes_received = -1;
            try
            {
                bytes_received = co_await socket.recvfrom(std::span<char>(buffer.data(), buffer.size()), sender_info);
            }
            catch (...)
            {
                // UDP is a little unreliable so shutdown hook might not be sent thus EBADFD will be sent and exception will be raised here
                if (source.stop_requested())
                    co_return;
                std::rethrow_exception(std::current_exception());
            }
            if (source.stop_requested())
            {
                break;
            }

            if (bytes_received > 0)
            {
                co_await socket.sendto(std::span<const char>(buffer.data(), bytes_received), sender_info);
            }
        }
    }

    task<void> shutdown()
    {
        source.request_stop();
        std::cout << "Requesting stop" << std::endl;

        udp_socket sock = make_udp_socket(version);
        const char dummy = '\0';
        co_await sock.sendto(std::span<const char>(&dummy, 1), info);
        co_await sock.close();

        std::cout << "Closing socket" << std::endl;
        co_await socket.close();
        std::cout << "Socket closed" << std::endl;
    }
};

TEST_CASE(TestAsyncUdpSocket)
{
    runtime_context context;
    std::cout << "Starting UDP Echo Server on " << info.host << ":" << info.port << std::endl;
    webcraft::test::udp::echo_server server(info);

    std::cout << "Creating Async UDP Socket for server at " << info.host << ":" << info.port << std::endl;

    auto task_fn = co_async
    {
        async_udp_echo_client client(version);
        const std::string message = "Hello, Async UDP Echo!";

        for (size_t i = 0; i < 5; ++i)
        {
            std::cout << "Async UDP Echo Attempt " << (i + 1) << std::endl;
            bool success = co_await client.echo(message, info);
            EXPECT_TRUE(success) << "Async UDP Echo should succeeded on the " << (i + 1) << " attempt";
        }

        std::cout << "Closing Async UDP Echo Client" << std::endl;
        co_await client.close();
    };

    sync_wait(task_fn());

    std::cout << "Shutting down UDP Echo Server" << std::endl;
    server.close();
}

TEST_CASE(TestAsyncUdpServer)
{
    runtime_context context;
    std::cout << "Starting Async UDP Echo Server on " << info.host << ":" << info.port << std::endl;
    async_udp_echo_server server(info);

    auto server_task = server.run();

    std::cout << "Creating UDP Socket for server at " << info.host << ":" << info.port << std::endl;

    webcraft::test::udp::echo_client client(version);

    const std::string message = "Hello, Async UDP Echo Server!";

    for (size_t i = 0; i < 5; ++i)
    {
        std::cout << "UDP Echo Attempt " << (i + 1) << std::endl;
        bool success = client.echo(message, info);
        EXPECT_TRUE(success) << "UDP Echo should succeeded on the " << (i + 1) << " attempt";
    }

    std::cout << "Closing UDP Echo Client" << std::endl;
    client.close();

    std::cout << "Shutting down Async UDP Echo Server" << std::endl;
    sync_wait(server.shutdown());
    std::cout << "Finishing task" << std::endl;

    sync_wait(server_task);
    std::cout << "Tearing down runtime" << std::endl;
}

TEST_CASE(TestAsyncUdpServerWithAsyncClient)
{
    runtime_context context;
    std::cout << "Starting Async UDP Echo Server on " << info.host << ":" << info.port << std::endl;
    async_udp_echo_server server(info);

    auto server_task = server.run();

    std::cout << "Creating UDP Socket for server at " << info.host << ":" << info.port << std::endl;

    auto task_fn = co_async
    {
        async_udp_echo_client client(version);
        const std::string message = "Hello, Async UDP Echo!";

        for (size_t i = 0; i < 5; ++i)
        {
            std::cout << "Async UDP Echo Attempt " << (i + 1) << std::endl;
            bool success = co_await client.echo(message, info);
            EXPECT_TRUE(success) << "Async UDP Echo should succeeded on the " << (i + 1) << " attempt";
        }

        std::cout << "Closing Async UDP Echo Client" << std::endl;
        co_await client.close();
    };

    sync_wait(task_fn());

    std::cout << "Shutting down Async UDP Echo Server" << std::endl;
    sync_wait(server.shutdown());
    std::cout << "Finishing task" << std::endl;

    sync_wait(server_task);
    std::cout << "Tearing down runtime" << std::endl;
}
TEST_CASE(TestUdpExplicitCloseIsIdempotent)
{
    runtime_context context;
    auto socket = make_udp_socket(ip_version::IPv4);
    socket.bind({"127.0.0.1", 0});
    TEST_TRACE("closing bound UDP socket without pending receives");
    sync_wait(socket.close());
    EXPECT_NO_THROW(sync_wait(socket.close()));
}
