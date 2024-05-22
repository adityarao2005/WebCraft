#pragma once

#include "webcraft/webcraft.h"
#include "webcraft/util/debug.h"
#include <memory>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN

/* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  /* Windows XP. */
#endif

#define NOGDI

#include <Ws2tcpip.h>
#include <winsock2.h>

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

#ifndef _WIN32
			// Linux
			typedef int SOCKET;
#endif
			/// <summary>
			/// Represents the base class of all sockets
			/// </summary>
			class SocketBase {
			protected:
				/// <summary>
				/// The native socket handle
				/// </summary>
				SOCKET handle;

				/// <summary>
				/// Creates an address based on the parameters
				/// </summary>
				/// <param name="host">Host name (nullptr if server)</param>
				/// <param name="port">port to bind or connect</param>
				/// <param name="result">pointer to address to be created</param>
				/// <param name="server">tells whether server or not</param>
				void create_address(const char* host, int port, addrinfo** result, bool server) const;
			public:
				/// <summary>
				/// Initializes the socket
				/// </summary>
				SocketBase();
				/// <summary>
				/// Able to create a socket from a handle
				/// </summary>
				/// <param name="handle">handle to be moved</param>
				SocketBase(SOCKET handle) : handle(handle) {}
				/// <summary>
				/// Destroys the socket
				/// </summary>
				~SocketBase();
			};

			/// <summary>
			/// Represents a socket
			/// </summary>
			class Socket : public SocketBase {
			public:
				/// <summary>
				/// Socket constructor
				/// </summary>
				Socket() : SocketBase() {}

				/// <summary>
				/// Socket constructor
				/// </summary>
				Socket(SOCKET handle) : SocketBase(handle) {}


				/// <summary>
				/// Connects to server at host:port
				/// </summary>
				/// <param name="host">Host of server</param>
				/// <param name="port">Port of server</param>
				void connect(std::string host, int port) const;
				/// <summary>
				/// Sends a packet of data to the other end
				/// </summary>
				/// <param name="data">Data packet to be sent</param>
				/// <param name="length">Length of the packet</param>
				/// <returns>how many bytes were sent</returns>
				int send(const char* data, int length) const;

				/// <summary>
				/// Receives a packet of data from the other end
				/// </summary>
				/// <param name="buffer">Buffer to store data packet</param>
				/// <param name="length">Length of available buffer space</param>
				/// <returns>Length of data packet stored</returns>
				int receive(char* buffer, int length) const;

				/// <summary>
				/// Shuts down the write access for socket
				/// </summary>
				void shutdown() const;

				inline SOCKET getHandle() const { return handle; }

				/// <summary>
				/// Closes the socket
				/// </summary>
				~Socket() {
					Debug::log("Destroyed");
				}

			};

			/// <summary>
			/// Represents a server socket
			/// </summary>
			class ServerSocket : public SocketBase {
			public:
				/// <summary>
				/// Server socket constructor
				/// </summary>
				ServerSocket() : SocketBase() {}

				/// <summary>
				/// Server socket constructor
				/// </summary>
				ServerSocket(SOCKET handle) : SocketBase(handle) {}

				/// <summary>
				/// Server socket destructor
				/// </summary>
				~ServerSocket() {}

				/// <summary>
				/// Binds server socket to a port
				/// </summary>
				/// <param name="port">port to be bound</param>
				void bind(int port);

				/// <summary>
				/// Listens for incoming connections
				/// </summary>
				void listen();

				/// <summary>
				/// Accepts an incoming connection
				/// </summary>
				/// <returns>Socket peer representing our end of connection</returns>
				std::shared_ptr<Socket> accept();
			};



		}
	}
}