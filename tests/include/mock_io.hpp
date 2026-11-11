#pragma once

#include <atomic>
#include <webcraft/async/thread_pool.hpp>
#include <webcraft/async/io/socket.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

#define SHUT_RDWR SD_BOTH

#else
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define WSAGetLastError() errno
#define closesocket ::close
#define SOCKET_ERROR -1
#endif

namespace webcraft::test
{

    namespace udp
    {
        using connection_info = webcraft::async::io::socket::connection_info;
        using ip_version = webcraft::async::io::socket::ip_version;

        class echo_server
        {
        private:
            SOCKET socket;
            std::jthread server_thread;

            void run(std::stop_token st)
            {
                while (!st.stop_requested())
                {
                    char buffer[1024];
                    struct sockaddr_storage client_addr;
                    socklen_t client_addr_len = sizeof(client_addr);
                    int bytes_received = recvfrom(socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
                    if (bytes_received == SOCKET_ERROR)
                    {
                        if (WSAGetLastError() == EINTR)
                        {
                            continue; // Interrupted by signal, retry
                        }
                        else
                        {
                            break; // Other error, exit the loop
                        }
                    }

                    if (st.stop_requested())
                    {
                        break; // Connection closed by client
                    }

                    int bytes_sent = sendto(socket, buffer, bytes_received, 0, (struct sockaddr *)&client_addr, client_addr_len);
                    if (bytes_sent == SOCKET_ERROR)
                    {
                        break; // Error sending data, exit the loop
                    }
                }
            }

            std::string host;
            uint16_t port;

        public:
            echo_server(const connection_info &info) : socket(INVALID_SOCKET), host(info.host), port(info.port)
            {
                struct addrinfo hints{};
                struct addrinfo *result, *ptr;
                hints.ai_family = AF_UNSPEC;
                hints.ai_socktype = SOCK_DGRAM;
                hints.ai_protocol = IPPROTO_UDP;

                int res = 0;
                if ((res = getaddrinfo(info.host.c_str(), std::to_string(info.port).c_str(), &hints, &result)) != 0)
                {
                    throw std::runtime_error("getaddrinfo failed with error: " + std::to_string(res) + " gai error: " + std::string(gai_strerror(res)));
                }

                for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
                {
                    socket = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                    if (socket == INVALID_SOCKET)
                    {
                        continue;
                    }

                    if (bind(socket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR)
                    {
                        closesocket(socket);
                        socket = INVALID_SOCKET;
                        continue;
                    }
                    break;
                }

                freeaddrinfo(result);
                if (socket == INVALID_SOCKET)
                {
                    throw std::runtime_error("Failed to create and bind socket with error: " + std::to_string(WSAGetLastError()));
                }

                server_thread = std::jthread([this](std::stop_token st)
                                             { run(st); });
            }

            ~echo_server()
            {
                close();
            }

            void close()
            {

                if (socket != INVALID_SOCKET)
                {
                    TEST_TRACE("UDP echo server: request stop");
                    server_thread.request_stop();

                    struct sockaddr_storage addr{};
                    socklen_t addr_len = sizeof(addr);

                    if (getsockname(socket, (struct sockaddr *)&addr, &addr_len) == -1)
                    {
                        throw std::runtime_error("getsockname failed");
                    }

                    const char o = '\0';
                    sendto(socket, &o, 1, 0, (struct sockaddr *)&addr, addr_len);

                    closesocket(socket);
                    socket = INVALID_SOCKET;

                    if (server_thread.joinable())
                    {
                        TEST_TRACE("UDP echo server: joining receive thread");
                        server_thread.join();
                        TEST_TRACE("UDP echo server: receive thread joined");
                    }
                }
            }

            echo_server(const echo_server &) = delete;
            echo_server &operator=(const echo_server &) = delete;
        };

        class echo_client
        {
        private:
            SOCKET socket;

        public:
            echo_client(std::optional<ip_version> version = std::nullopt) : socket(INVALID_SOCKET)
            {
                if (version == ip_version::IPv4)
                {
                    socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                }
                else if (version == ip_version::IPv6)
                {
                    socket = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
                }
                else
                {
                    // default to ipv6 if none is specified, fallback to ipv4 if ipv6 is not available
                    socket = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
                    if (socket == INVALID_SOCKET)
                    {
                        socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    }
                }

                if (socket == INVALID_SOCKET)
                {
                    throw std::runtime_error("Failed to create socket");
                }

                std::cout << "UDP Echo Client created with socket: " << socket << std::endl;
            }

            ~echo_client()
            {
                close();
            }

            void close()
            {

                if (socket != INVALID_SOCKET)
                {
                    closesocket(socket);
                    socket = INVALID_SOCKET;
                }
            }

            echo_client(const echo_client &) = delete;
            echo_client &operator=(const echo_client &) = delete;

            /// @brief Expects an echo to be sent and received
            /// @param message the message to be echo'd
            /// @return true if the echo was successful, false otherwise
            bool echo(const std::string &message, const connection_info &info)
            {
                struct addrinfo hints{};
                struct addrinfo *result, *ptr;
                hints.ai_family = AF_UNSPEC;
                hints.ai_socktype = SOCK_DGRAM;
                hints.ai_protocol = IPPROTO_UDP;

                if (getaddrinfo(info.host.c_str(), std::to_string(info.port).c_str(), &hints, &result) != 0)
                {
                    throw std::runtime_error("getaddrinfo failed");
                }

                ptr = result; // Use the first result

                int bytes_sent = sendto(socket, message.c_str(), (int)message.size(), 0, ptr->ai_addr, (int)ptr->ai_addrlen);
                if (bytes_sent == SOCKET_ERROR)
                {
                    freeaddrinfo(result);
                    return false; // Error sending data
                }

                char buffer[1024];
                struct sockaddr_storage from_addr;
                socklen_t from_addr_len = sizeof(from_addr);
                int bytes_received = recvfrom(socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&from_addr, &from_addr_len);
                if (bytes_received == SOCKET_ERROR)
                {
                    freeaddrinfo(result);
                    return false; // Error receiving data
                }

                buffer[bytes_received] = '\0'; // Null-terminate the received data
                freeaddrinfo(result);

                return message == std::string(buffer, bytes_received); // Check if the received message matches the sent message
            }
        };
    }

    namespace tcp
    {
        using connection_info = webcraft::async::io::socket::connection_info;

        class echo_server
        {
        private:
            SOCKET socket;
            webcraft::async::thread_pool pool;
            std::jthread server_thread;

            void handle_client(SOCKET client_socket, std::stop_token token)
            {
                char buffer[1024];
                while (!token.stop_requested())
                {
                    std::cout << "### Server > Receiving data from client" << std::endl;
                    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
                    if (bytes_received == SOCKET_ERROR || bytes_received == 0)
                    {
                        break; // Error or connection closed by client
                    }

                    std::cout << "### Server > Data received: " << bytes_received << " bytes" << std::string(buffer, bytes_received) << std::endl;

                    if (token.stop_requested())
                    {
                        std::cout << "Stop has been requested" << std::endl;
                        break; // Connection closed by client
                    }

                    std::cout << "### Server > Sending the data :" << std::string(buffer, bytes_received) << std::endl;
                    int bytes_sent = send(client_socket, buffer, bytes_received, 0);
                    if (bytes_sent == SOCKET_ERROR)
                    {
                        break; // Error sending data
                    }
                    std::cout << "### Server > Data sent: " << bytes_sent << std::endl;
                }

                closesocket(client_socket);
            }

            void run(std::stop_token token)
            {
                while (!token.stop_requested())
                {
                    struct sockaddr_storage client_addr;
                    socklen_t client_addr_len = sizeof(client_addr);
                    SOCKET client_socket = accept(socket, (struct sockaddr *)&client_addr, &client_addr_len);
                    if (client_socket == INVALID_SOCKET)
                    {
                        if (WSAGetLastError() == EINTR)
                        {
                            continue; // Interrupted by signal, retry
                        }
                        else
                        {
                            break; // Other error, exit the loop
                        }
                    }

                    if (token.stop_requested())
                    {
                        closesocket(client_socket);
                        break; // Connection closed by client
                    }

                    pool.submit([this, client_socket, token]
                                { handle_client(client_socket, token); });
                }
            }

        public:
            echo_server(const connection_info &info) : socket(INVALID_SOCKET), pool(2, 8)
            {
                struct addrinfo hints{};
                struct addrinfo *result, *ptr;
                hints.ai_family = AF_UNSPEC;
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_protocol = IPPROTO_TCP;
                hints.ai_flags = AI_PASSIVE;

                if (getaddrinfo(info.host.c_str(), std::to_string(info.port).c_str(), &hints, &result) != 0)
                {
                    throw std::runtime_error("getaddrinfo failed");
                }

                for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
                {
                    socket = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                    if (socket == INVALID_SOCKET)
                    {
                        continue;
                    }

#ifndef _WIN32
                    int opt = 1;
                    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#endif
                    if (bind(socket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR)
                    {
                        closesocket(socket);
                        socket = INVALID_SOCKET;
                        continue;
                    }

                    if (listen(socket, SOMAXCONN) == SOCKET_ERROR)
                    {
                        closesocket(socket);
                        socket = INVALID_SOCKET;
                        continue;
                    }
                    break;
                }

                freeaddrinfo(result);
                if (socket == INVALID_SOCKET)
                {
                    throw std::runtime_error("Failed to create and bind socket");
                }

                server_thread = std::jthread([this](std::stop_token st)
                                             { run(st); });
            }

            ~echo_server()
            {
                close();
            }

            void close()
            {

                if (socket != INVALID_SOCKET)
                {
                    server_thread.request_stop();

                    pool.try_shutdown();

                    struct sockaddr_storage addr{};
                    socklen_t addr_len = sizeof(addr);

                    if (getsockname(socket, (struct sockaddr *)&addr, &addr_len) == -1)
                    {
                        throw std::runtime_error("getsockname failed");
                    }

                    SOCKET wakeup = ::socket(addr.ss_family, SOCK_STREAM, IPPROTO_TCP);
                    if (wakeup == INVALID_SOCKET)
                    {
                        throw std::runtime_error("Failed to create wakeup socket");
                    }

                    if (connect(wakeup, (struct sockaddr *)&addr, addr_len) == SOCKET_ERROR)
                    {
                        closesocket(wakeup);
                        throw std::runtime_error("Failed to connect wakeup socket");
                    }

                    closesocket(wakeup);
                    closesocket(socket);
                    socket = INVALID_SOCKET;

                    if (server_thread.joinable())
                    {
                        server_thread.join();
                    }
                }
            }

            echo_server(const echo_server &) = delete;
            echo_server &operator=(const echo_server &) = delete;
        };

        class echo_client
        {
        private:
            SOCKET socket;

        public:
            echo_client(const connection_info &info) : socket(INVALID_SOCKET)
            {
                struct addrinfo hints{};
                struct addrinfo *result, *ptr;
                hints.ai_family = AF_UNSPEC;
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_protocol = IPPROTO_TCP;

                if (getaddrinfo(info.host.c_str(), std::to_string(info.port).c_str(), &hints, &result) != 0)
                {
                    throw std::runtime_error("getaddrinfo failed");
                }

                for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
                {
                    socket = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                    if (socket == INVALID_SOCKET)
                    {
                        continue;
                    }

                    if (connect(socket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR)
                    {
                        closesocket(socket);
                        socket = INVALID_SOCKET;
                        continue;
                    }
                    break;
                }

                freeaddrinfo(result);
                if (socket == INVALID_SOCKET)
                {
                    throw std::runtime_error("Failed to create and connect socket");
                }
            }

            ~echo_client()
            {
                close();
            }

            void close()
            {
                if (socket != INVALID_SOCKET)
                {
                    ::shutdown(socket, SHUT_RDWR);
                    closesocket(socket);
                    socket = INVALID_SOCKET;
                }
            }

            echo_client(const echo_client &) = delete;
            echo_client &operator=(const echo_client &) = delete;

            bool echo(const std::string &message)
            {
                int bytes_sent = send(socket, message.c_str(), (int)message.size(), 0);
                if (bytes_sent == SOCKET_ERROR)
                {
                    return false; // Error sending data
                }

                char buffer[1024];
                int bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received == SOCKET_ERROR || bytes_received == 0)
                {
                    return false; // Error receiving data or connection closed
                }

                buffer[bytes_received] = '\0'; // Null-terminate the received data

                return message == std::string(buffer, bytes_received); // Check if the received message matches the sent message
            }
        };
    }
}