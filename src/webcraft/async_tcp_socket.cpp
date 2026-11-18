///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <webcraft/async/io/io.hpp>
#include <webcraft/async/runtime.hpp>
#include <webcraft/async/task_completion_source.hpp>
#include <webcraft/async/runtime/windows.event.hpp>
#include <webcraft/async/runtime/macos.event.hpp>
#include <webcraft/async/runtime/linux.event.hpp>
#include <cstdio>
#include <webcraft/async/async_event.hpp>
#include <system_error>
#include "async_tcp_socket_decl.hpp"

using namespace webcraft::async;
using namespace webcraft::async::io::socket::detail;

#if defined(WEBCRAFT_MOCK_SOCKET_TESTS)

inline std::shared_ptr<tcp_socket_descriptor>
webcraft::async::io::socket::detail::make_tcp_socket_descriptor()
{
    throw std::runtime_error("TCP socket descriptor not implemented in mock tests");
}

#elif defined(__linux__)

#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

io_uring_tcp_socket_descriptor::io_uring_tcp_socket_descriptor()
{
    fd = -1;
}

io_uring_tcp_socket_descriptor::io_uring_tcp_socket_descriptor(int fd, std::string host, uint16_t port) : fd(fd), host(std::move(host)), port(port)
{
}

io_uring_tcp_socket_descriptor::~io_uring_tcp_socket_descriptor()
{
    fire_and_forget(close());
}

task<void> io_uring_tcp_socket_descriptor::close()
{
    if (fd == -1)
        co_return;

    bool expected = false;
    if (closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        int fd = this->fd;

        co_await webcraft::async::detail::as_awaitable(webcraft::async::detail::linux::create_io_uring_event([fd](struct io_uring_sqe *sqe)
                                                                                                             { io_uring_prep_close(sqe, fd); }));
    }
}

task<size_t> io_uring_tcp_socket_descriptor::read(std::span<char> buffer)
{
    int fd = this->fd;
    auto event = webcraft::async::detail::as_awaitable(webcraft::async::detail::linux::create_io_uring_event([fd, buffer](struct io_uring_sqe *sqe)
                                                                                                             { io_uring_prep_recv(sqe, fd, buffer.data(), buffer.size(), 0); }));

    co_await event;

    if (event.get_result() < 0)
    {
        std::error_code ec(-event.get_result(), std::system_category());
        throw std::system_error(ec, "Failed to read from socket");
    }

    co_return event.get_result();
}

task<size_t> io_uring_tcp_socket_descriptor::write(std::span<const char> buffer)
{
    int fd = this->fd;
    auto event = webcraft::async::detail::as_awaitable(webcraft::async::detail::linux::create_io_uring_event([fd, buffer](struct io_uring_sqe *sqe)
                                                                                                             { io_uring_prep_send(sqe, fd, buffer.data(), buffer.size(), MSG_NOSIGNAL); }));

    co_await event;

    if (event.get_result() < 0)
    {
        std::error_code ec(-event.get_result(), std::system_category());
        throw std::system_error(ec, "Failed to write to socket");
    }

    co_return event.get_result();
}

task<void> io_uring_tcp_socket_descriptor::connect(const webcraft::async::io::socket::connection_info &info)
{

    this->host = info.host;
    this->port = info.port;

    // Prepare address string for getaddrinfo
    std::string port_str = std::to_string(info.port);
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *res;
    int ret = getaddrinfo(info.host.c_str(), port_str.c_str(), &hints, &res);
    if (ret != 0)
    {
        throw webcraft::net::util::get_addr_info_error(ret);
    }

    bool flag = false;
    for (auto *rp = res; rp; rp = rp->ai_next)
    {

        int family = rp->ai_family;
        int sock_type = rp->ai_socktype;
        int protocol = rp->ai_protocol;
        const sockaddr *addr = rp->ai_addr;
        const socklen_t len = rp->ai_addrlen;

        int fd = socket(family, sock_type, protocol);
        if (fd < 0)
        {
            continue;
        }

        // Await io_uring connect
        auto event = webcraft::async::detail::as_awaitable(
            webcraft::async::detail::linux::create_io_uring_event(
                [fd, addr, len](struct io_uring_sqe *sqe)
                {
                    io_uring_prep_connect(sqe, fd, addr, len);
                }));

        co_await event;

        if (event.get_result() < 0)
        {
            ::close(fd);
        }
        else
        {
            this->fd = fd;
            flag = true;
            break;
        }
    }

    freeaddrinfo(res); // Free memory allocated by getaddrinfo

    if (!flag)
    {
        std::error_code ec(errno, std::system_category());
        throw std::system_error(ec, "Failed to create socket");
    }
}

void io_uring_tcp_socket_descriptor::shutdown(webcraft::async::io::socket::socket_stream_mode mode)
{
    int fd = this->fd;

    if (mode == webcraft::async::io::socket::socket_stream_mode::READ)
    {
        ::shutdown(fd, SHUT_RD);
    }
    else
    {
        ::shutdown(fd, SHUT_WR);
    }
}

std::string io_uring_tcp_socket_descriptor::get_remote_host()
{
    return host;
}

uint16_t io_uring_tcp_socket_descriptor::get_remote_port()
{
    return port;
}

std::shared_ptr<tcp_socket_descriptor> webcraft::async::io::socket::detail::make_tcp_socket_descriptor()
{
    return std::make_shared<io_uring_tcp_socket_descriptor>();
}

#elif defined(_WIN32)

iocp_tcp_socket_descriptor::iocp_tcp_socket_descriptor() : socket(INVALID_SOCKET), iocp(INVALID_HANDLE_VALUE), port(0)
{
    get_extension_manager(); // Ensure WSA extensions are loaded
}

iocp_tcp_socket_descriptor::iocp_tcp_socket_descriptor(SOCKET sock, std::string host, uint16_t port) : socket(sock), iocp(INVALID_HANDLE_VALUE), host(std::move(host)), port(port)
{
    // Associate this socket handle with the global IOCP
    iocp = ::CreateIoCompletionPort((HANDLE)socket, (HANDLE)webcraft::async::detail::get_native_handle(), 0, 0);

    if (iocp == nullptr)
    {
        ::closesocket(socket);
        throw webcraft::async::detail::windows::overlapped_runtime_event_error("Failed to associate socket with IO completion port: " + std::to_string(GetLastError()));
    }
}

iocp_tcp_socket_descriptor::~iocp_tcp_socket_descriptor()
{
    fire_and_forget(close());
}

task<void> iocp_tcp_socket_descriptor::connect(const webcraft::async::io::socket::connection_info &info)
{
    this->host = info.host;
    this->port = info.port;

    // Prepare address string for getaddrinfo
    std::string port_str = std::to_string(info.port);
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *res;
    int ret = getaddrinfo(info.host.c_str(), port_str.c_str(), &hints, &res);
    if (ret != 0)
    {
        throw webcraft::net::util::get_addr_info_error(ret);
    }

    bool flag = false;
    for (auto *rp = res; rp; rp = rp->ai_next)
    {

        int family = rp->ai_family;
        int sock_type = rp->ai_socktype;
        int protocol = rp->ai_protocol;
        const sockaddr *addr = rp->ai_addr;
        const socklen_t len = (socklen_t)rp->ai_addrlen;

        SOCKET fd = ::socket(family, sock_type, protocol);
        if (fd == INVALID_SOCKET)
        {
            continue;
        }

        iocp = ::CreateIoCompletionPort((HANDLE)fd, (HANDLE)webcraft::async::detail::get_native_handle(), 0, 0);

        if (iocp == nullptr)
        {
            ::closesocket(fd);
            throw webcraft::async::detail::windows::overlapped_runtime_event_error("Failed to associate socket with IO completion port");
        }

        int result = ::connect(fd, res->ai_addr, (int)res->ai_addrlen);
        if (result == SOCKET_ERROR)
        {
            ::closesocket(fd);
            continue;
        }
        else
        {
            this->socket = fd;
            flag = true;
            break;
        }
    }

    freeaddrinfo(res); // Free memory allocated by getaddrinfo

    if (!flag)
        throw webcraft::async::detail::windows::overlapped_winsock2_runtime_error("Failed to create socket");

    co_return;
}

task<size_t> iocp_tcp_socket_descriptor::read(std::span<char> buffer)
{
    SOCKET fd = this->socket;
    WSABUF wsabuf;
    wsabuf.buf = const_cast<char *>(buffer.data());
    wsabuf.len = (ULONG)buffer.size();
    DWORD flags = 0;

    auto event = webcraft::async::detail::as_awaitable(
        webcraft::async::detail::windows::create_async_socket_overlapped_event(
            fd,
            [fd, &wsabuf, &flags](LPDWORD numberOfBytesTransfered, LPOVERLAPPED overlapped)
            {
                return WSARecv(fd, &wsabuf, 1, numberOfBytesTransfered, &flags, overlapped, nullptr);
            }));

    co_await event;

    if (event.get_result() < 0)
    {
        std::error_code ec(-event.get_result(), std::system_category());
        throw std::system_error(ec, "Failed to read from socket");
    }

    co_return event.get_result();
}

task<size_t> iocp_tcp_socket_descriptor::write(std::span<const char> buffer)
{

    SOCKET fd = this->socket;
    WSABUF wsabuf;
    wsabuf.buf = const_cast<char *>(buffer.data());
    wsabuf.len = (ULONG)buffer.size();

    auto event = webcraft::async::detail::as_awaitable(
        webcraft::async::detail::windows::create_async_socket_overlapped_event(
            fd,
            [fd, &wsabuf](LPDWORD numberOfBytesTransfered, LPWSAOVERLAPPED overlapped)
            {
                return WSASend(fd, &wsabuf, 1, numberOfBytesTransfered, 0, overlapped, nullptr);
            }));

    co_await event;

    if (event.get_result() < 0)
    {
        std::error_code ec(-event.get_result(), std::system_category());
        throw std::system_error(ec, "Failed to write to socket");
    }

    co_return event.get_result();
}

void iocp_tcp_socket_descriptor::shutdown(webcraft::async::io::socket::socket_stream_mode mode)
{
    if (mode == webcraft::async::io::socket::socket_stream_mode::READ)
    {
        ::shutdown(socket, SD_RECEIVE);
    }
    else
    {
        ::shutdown(socket, SD_SEND);
    }
}

std::string iocp_tcp_socket_descriptor::get_remote_host()
{
    return host;
}

uint16_t iocp_tcp_socket_descriptor::get_remote_port()
{
    return port;
}

task<void> iocp_tcp_socket_descriptor::close()
{
    if (socket != INVALID_SOCKET)
    {
        ::closesocket(socket);
        socket = INVALID_SOCKET;
    }

    // Don't close iocp as it's the global IOCP handle
    co_return;
}

std::shared_ptr<tcp_socket_descriptor> webcraft::async::io::socket::detail::make_tcp_socket_descriptor()
{
    return std::make_shared<iocp_tcp_socket_descriptor>();
}

#elif defined(__APPLE__)

kqueue_tcp_socket_descriptor::kqueue_tcp_socket_descriptor()
{
    fd = -1;
}

kqueue_tcp_socket_descriptor::kqueue_tcp_socket_descriptor(int fd, std::string host, uint16_t port) : fd(fd), host(std::move(host)), port(port)
{
    register_with_queue();
}

kqueue_tcp_socket_descriptor ::~kqueue_tcp_socket_descriptor()
{
    fire_and_forget(close());
}

task<void> kqueue_tcp_socket_descriptor::close()
{
    if (fd == -1)
        co_return;

    bool expected = false;
    if (closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        int fd = this->fd;
        deregister_with_queue();
        ::close(fd);
    }
}

task<size_t> kqueue_tcp_socket_descriptor::read(std::span<char> buffer)
{
    if (no_more_bytes)
        co_return 0;

    if (buffer.size() <= read_buffer.size())
    {
        std::copy(read_buffer.begin(), read_buffer.begin() + buffer.size(), buffer.begin());

        // remove the read portions of the read buffer
        read_buffer.erase(read_buffer.begin(), read_buffer.begin() + buffer.size());
        co_return buffer.size();
    }

    co_await read_event;

    if (no_more_bytes)
        co_return 0;

    // copy what can fit into the buffer
    auto min_read = std::min(buffer.size(), read_buffer.size());
    std::copy(read_buffer.begin(), read_buffer.begin() + min_read, buffer.begin());

    // remove the read portions of the read buffer
    read_buffer.erase(read_buffer.begin(), read_buffer.begin() + min_read);

    co_return min_read;
}

task<size_t> kqueue_tcp_socket_descriptor::write(std::span<const char> buffer)
{
    write_buffer.insert(write_buffer.end(), buffer.begin(), buffer.end());

    // drain the write buffer
    while (write_buffer.size() > 0)
        co_await write_event;

    // Look at the difference in size of the buffer
    co_return buffer.size();
}

task<void> kqueue_tcp_socket_descriptor::connect(const webcraft::async::io::socket::connection_info &info)
{

    this->host = info.host;
    this->port = info.port;

    // Prepare address string for getaddrinfo
    std::string port_str = std::to_string(info.port);
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *res;
    int ret = getaddrinfo(info.host.c_str(), port_str.c_str(), &hints, &res);
    if (ret != 0)
    {
        throw webcraft::net::util::get_addr_info_error(ret);
    }

    bool flag = false;
    for (auto *rp = res; rp; rp = rp->ai_next)
    {

        int family = rp->ai_family;
        int sock_type = rp->ai_socktype;
        int protocol = rp->ai_protocol;
        const sockaddr *addr = rp->ai_addr;
        const socklen_t len = rp->ai_addrlen;

        int fd = socket(family, sock_type, protocol);
        if (fd < 0)
        {
            continue;
        }
        if (::connect(fd, rp->ai_addr, rp->ai_addrlen) < 0)
        {
            ::close(fd);
        }
        else
        {
            this->fd = fd;
            flag = true;
            break;
        }
    }

    freeaddrinfo(res); // Free memory allocated by getaddrinfo

    if (!flag) {
        std::error_code ec(errno, std::system_category());
        throw std::system_error(ec, "Failed to create socket");
    }
    
    register_with_queue();
    co_return;
}

void kqueue_tcp_socket_descriptor::shutdown(webcraft::async::io::socket::socket_stream_mode mode)
{
    int fd = this->fd;

    if (mode == webcraft::async::io::socket::socket_stream_mode::READ)
    {
        ::shutdown(fd, SHUT_RD);
    }
    else
    {
        ::shutdown(fd, SHUT_WR);
    }
}

std::string kqueue_tcp_socket_descriptor::get_remote_host()
{
    return host;
}

uint16_t kqueue_tcp_socket_descriptor::get_remote_port()
{
    return port;
}

void kqueue_tcp_socket_descriptor::register_with_queue()
{
    // make socket non-blocking
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        throw webcraft::async::detail::macos::kqueue_runtime_error("Error in getting socket flags");
    }
    int res = ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (res == -1)
    {
        throw webcraft::async::detail::macos::kqueue_runtime_error("Error in setting non-blocking flag");
    }

    kq = (int)webcraft::async::detail::get_native_handle();

    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, (webcraft::async::detail::runtime_callback *)this);
    if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1)
    {
        throw webcraft::async::detail::macos::kqueue_runtime_error("Could not register read listener");
    }

    EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD, 0, 0, (webcraft::async::detail::runtime_callback *)this);
    if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1)
    {
        throw webcraft::async::detail::macos::kqueue_runtime_error("Could not register write listener");
    }
}

void kqueue_tcp_socket_descriptor::deregister_with_queue()
{
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1)
    {
        std::cerr << "Could not unregister listener" << std::endl;
    }

    EV_SET(&kev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1)
    {
        std::cerr << "Could not unregister listener" << std::endl;
    }

    no_more_bytes = true;
    read_event.notify();
}

void kqueue_tcp_socket_descriptor::try_execute(int result, bool cancelled)
{
    if (closed)
        return;

    auto filter = webcraft::async::detail::get_kqueue_filter();
    auto flags = webcraft::async::detail::get_kqueue_flags();

    if (filter == EVFILT_READ)
    {
        if (no_more_bytes)
            return;

        std::array<char, 1024> buffer{};
        while (true)
        {
            int bytes_read = ::recv(fd, buffer.data(), buffer.size(), 0);
            if (bytes_read < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else {
                    if (errno == ECONNRESET)
                    {
                        no_more_bytes = true;
                        break;
                    }
                    else {
                        std::error_code ec(errno, std::system_category());
                        throw std::system_error(ec, "This should not have happened but read failed");
                    }
                }
            }

            if (bytes_read == 0)
                break;

            read_buffer.insert(read_buffer.end(), buffer.begin(), buffer.begin() + bytes_read);
        }

        read_event.notify();
    }

    if (filter == EVFILT_WRITE)
    {
        if (!write_buffer.empty())
        {
            int bytes_written = ::send(fd, write_buffer.data(), write_buffer.size(), 0);
            write_buffer.erase(write_buffer.begin(), write_buffer.begin() + bytes_written);
            write_event.notify();
        }
    }
}

std::shared_ptr<tcp_socket_descriptor> webcraft::async::io::socket::detail::make_tcp_socket_descriptor()
{
    return std::make_shared<kqueue_tcp_socket_descriptor>();
}

#endif
