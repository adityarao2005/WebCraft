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
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define SD_SEND SHUT_WR
#endif


			// Functions
			int SocketInit();
			int SocketCleanup();

			class SocketBase {
			protected:
				SOCKET handle;
			public:
				SocketBase() {
					// Create a SOCKET for connecting to server
					if ((handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
						Debug::throwException("socket failed with error");
					}
				}

				void create_address(const char* host, int port, addrinfo** result, bool server) {
					addrinfo hints;

					memset(&hints, 0, sizeof(hints));
					hints.ai_family = AF_INET;
					hints.ai_socktype = SOCK_STREAM;
					hints.ai_protocol = IPPROTO_TCP;
					if (server) {
						hints.ai_flags = AI_PASSIVE;
					}


					// Resolve the server address and port
					int iResult;
					if ((iResult = getaddrinfo(host, std::to_string(port).data(), &hints, result)) != 0) {
						Debug::throwException("getaddrinfo failed with error: " + std::to_string(iResult));
					}

				}

				SocketBase(SOCKET handle) : handle(handle) {}
				~SocketBase() {
					closesocket(handle);
				}
			};

			class Socket : public SocketBase {
			public:
				Socket() : SocketBase() {}

				Socket(SOCKET handle) : SocketBase(handle) {}

				void connect(std::string host, int port) {
					addrinfo* result = nullptr;
					create_address(host.data(), port, &result, false);

					// Connect to server.
					if (::connect(handle, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
						Debug::throwException("Unable to connect to server!");
					}
					// no longer need address info for server
					freeaddrinfo(result);

					// if connection failed
					if (handle == INVALID_SOCKET) {
						Debug::throwException("Unable to connect to server!");
					}
				}

				int send(const char* data, int length) {
					int iResult;
					// Send an initial buffer
					if ((iResult = ::send(handle, data, length, 0)) == SOCKET_ERROR) {
						Debug::throwException("send failed with error");
					}
					return iResult;
				}

				int receive(char* buffer, int length) {
					int iResult;
					// Receive into buffer with buffer length
					if ((iResult = ::recv(handle, buffer, length, 0)) == SOCKET_ERROR) {
						Debug::throwException("recv failed with error");
					}
					return iResult;
				}

				void shutdown() {
					// shutdown the connection since no more data will be sent
					if (::shutdown(handle, SD_SEND) == SOCKET_ERROR) {
						Debug::throwException("shutdown failed with error");
					}
				}

				~Socket() {}

			};

			class ServerSocket : public SocketBase {
			public:
				ServerSocket() : SocketBase() {}

				ServerSocket(SOCKET handle) : SocketBase(handle) {}

				~ServerSocket() {}

				void bind(int port) {
					addrinfo* result = nullptr;
					create_address(nullptr, port, &result, true);
					// Setup the TCP listening socket
					if (::bind(handle, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
						Debug::throwException("bind failed with error");
					}

					freeaddrinfo(result);
				}

				void listen() {
					if (::listen(handle, SOMAXCONN) == SOCKET_ERROR) {
						Debug::throwException("listen failed with error");
					}
				}

				Socket accept() {
					SOCKET clientSocket;
					// Accept a client socket
					if ((clientSocket = ::accept(handle, NULL, NULL)) == INVALID_SOCKET) {
						Debug::throwException("accept failed with error");
					}
					return Socket(clientSocket);
				}
			};



		}
	}
}