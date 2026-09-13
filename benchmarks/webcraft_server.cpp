///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/// @file webcraft_server.cpp
/// @brief Minimal HTTP "Hello, World!" server for benchmarking WebCraft.
///
/// This server is optimised for throughput measurement:
///   - No per-request stdout logging
///   - HTTP/1.1 keep-alive (reads in a loop on the same socket)
///   - Small, fixed-size response body

#include <iostream>
#include <atomic>
#include <webcraft/async/async.hpp>
#include <csignal>
#include <sstream>
#include <cstring>
#include <string_view>
#include <sys/socket.h>

using namespace webcraft::async;
using namespace webcraft::async::io::socket;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

static constexpr uint16_t DEFAULT_PORT = 8080;
static std::atomic<bool> shutdown_requested{false};
static connection_info conn{.host = "0.0.0.0", .port = DEFAULT_PORT};

// ---------------------------------------------------------------------------
// Pre-built HTTP response (computed once, served for every request)
// ---------------------------------------------------------------------------

static const std::string RESPONSE_BODY =
    "<!DOCTYPE html>\n"
    "<html><head><title>WebCraft Benchmark</title></head>\n"
    "<body><h1>Hello, World!</h1></body></html>\n";

static const std::string HTTP_RESPONSE =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: " + std::to_string(RESPONSE_BODY.size()) + "\r\n"
    "Connection: keep-alive\r\n"
    "\r\n" +
    RESPONSE_BODY;

// ---------------------------------------------------------------------------
// Signal handling — graceful shutdown
// ---------------------------------------------------------------------------

void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        shutdown_requested.store(true, std::memory_order_release);

        // Send a dummy connection to unblock the accept() call
        auto close_fn = co_async
        {
            tcp_socket dummy_socket = make_tcp_socket();
            co_await dummy_socket.connect(conn);
            co_await dummy_socket.close();
        };

        sync_wait(close_fn());
    }
}

// ---------------------------------------------------------------------------
// Per-connection handler (keep-alive loop)
// ---------------------------------------------------------------------------

async_t(void) handle_client(tcp_socket socket)
{
    auto &reader = socket.get_readable_stream();
    auto &writer = socket.get_writable_stream();

    char buffer[4096];

    try
    {
        while (true)
        {
            // Read the next HTTP request on this connection
            std::size_t n = co_await reader.recv(
                std::span<char>(buffer, sizeof(buffer)));

            if (n == 0)
                break; // Client closed the connection

            // We don't need to fully parse the request for a benchmark —
            // just check that we received *something* and send the canned
            // response.  A production server would parse method/path/headers.

            // Check for "Connection: close" header (simple substring search)
            std::string_view request(buffer, n);
            bool close_after = (request.find("Connection: close") != std::string_view::npos);

            // Send pre-built response
            co_await writer.send(
                std::span<const char>(HTTP_RESPONSE.data(), HTTP_RESPONSE.size()));

            if (close_after)
                break;
        }
    }
    catch (const std::exception &)
    {
        // Silently swallow errors during benchmark — connection resets,
        // broken pipes, etc. are expected under heavy load.
    }

    co_await socket.close();
}

// ---------------------------------------------------------------------------
// Main — server loop
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    // Allow overriding the port via CLI: ./webcraft_bench_server [port]
    if (argc >= 2)
    {
        conn.port = static_cast<uint16_t>(std::stoi(argv[1]));
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    runtime_context ctx;

    auto server_fn = co_async
    {
        tcp_listener listener = make_tcp_listener();
        listener.bind(conn);
        listener.listen(SOMAXCONN);

        std::cout << "WebCraft benchmark server listening on "
                  << conn.host << ":" << conn.port << std::endl;

        while (!shutdown_requested.load(std::memory_order_acquire))
        {
            auto peer = co_await listener.accept();

            if (shutdown_requested.load(std::memory_order_acquire))
                break;

            fire_and_forget(handle_client(std::move(peer)));
        }

        co_await listener.close();
    };

    sync_wait(server_fn());
    webcraft::async::detail::shutdown_runtime();

    std::cout << "Server exited cleanly." << std::endl;
    return 0;
}
