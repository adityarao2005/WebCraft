#pragma once

#include "webcraft/webcraft.h"
#include "webcraft/util/debug.h"


#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN

/* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  /* Windows XP. */
#endif

#include <winsock2.h>
#include <Ws2tcpip.h>


#else
/* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
#include <unistd.h> /* Needed for close() */

#endif

using Debug = WebCraft::Util::Debug;

namespace WebCraft {
	namespace Networking {
		namespace Sockets {
			// Typedefs
#ifndef _WIN32
// Linux
			typedef int SOCKET;
#endif

			// Functions
			int SocketInit();
			int SocketCleanup();

			class SocketBase {
			protected:
				SOCKET handle;

				void create_address(const char* host, int port, addrinfo** result, bool server);
			public:
				SocketBase();

				SocketBase(SOCKET handle) : handle(handle) {}
				~SocketBase();
			};

			class Socket : public SocketBase {
			public:
				Socket() : SocketBase() {}

				Socket(SOCKET handle) : SocketBase(handle) {}

				void connect(std::string host, int port);

				int send(const char* data, int length);

				int receive(char* buffer, int length);

				void shutdown();

				~Socket() {}

			};

			class ServerSocket : public SocketBase {
			public:
				ServerSocket() : SocketBase() {}

				ServerSocket(SOCKET handle) : SocketBase(handle) {}

				~ServerSocket() {}

				void bind(int port);

				void listen();

				Socket accept();
			};



		}
	}
}