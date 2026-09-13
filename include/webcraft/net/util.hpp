#pragma once

/**
 * @file net/util.hpp
 * @brief Cross-platform networking helper utilities.
 *
 * This header wraps common address-resolution and socket helper logic used
 * by async socket abstractions. Include it when implementing or extending
 * network-facing components that need these low-level helpers.
 */

#include <sstream>

#if defined(__linux__)

#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#elif defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

#elif defined(__APPLE__)

#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#endif

#include <webcraft/async/task.hpp>

#include <string.h>

namespace webcraft::net::util
{

    /**
     * @brief Exception thrown for address-resolution failures.
     */
    class get_addr_info_error : public std::exception
    {
    public:
        explicit get_addr_info_error(int ret)
        {
            std::stringstream ss;
            ss << "getaddrinfo failed: " << " (Error Code: " << ret << ")";
            message_ = ss.str();
        }
        virtual const char *what() const noexcept override
        {
            return message_.c_str();
        }

    private:
        std::string message_;
    };

    /**
     * @brief Converts a socket address into numeric host and port.
     */
    inline std::pair<std::string, uint16_t> addr_to_host_port(
        const struct sockaddr_storage &addr)
    {
        char host[NI_MAXHOST];
        char service[NI_MAXSERV];

        if (getnameinfo((struct sockaddr *)&addr, sizeof(addr),
                        host, sizeof(host),
                        service, sizeof(service),
                        NI_NUMERICHOST | NI_NUMERICSERV) != 0)
        {
            return {"", 0}; // Failed
        }

        return {std::string(host), static_cast<uint16_t>(std::stoi(service))};
    }

    using on_address_resolved = std::function<bool(sockaddr *addr, socklen_t addrlen)>;

    /**
     * @brief Resolves host/port and invokes a callback for each resolved address.
     * @return `true` if callback accepted one resolved address, otherwise `false`.
     */
    inline bool host_port_to_addr(const webcraft::async::io::socket::connection_info &info, on_address_resolved callback)
    {
        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM; // TCP

        struct addrinfo *res;
        int ret = getaddrinfo(info.host.c_str(), std::to_string(info.port).c_str(), &hints, &res);
        if (ret != 0)
        {
            return false;
        }

        // Call the callback with the resolved address
        bool check = false;

        for (auto *rp = res; rp; rp = rp->ai_next)
        {
            check = callback(rp->ai_addr, (socklen_t)rp->ai_addrlen);
            if (check)
                break;
            else
                continue;
        }

        freeaddrinfo(res);
        return check;
    }

}