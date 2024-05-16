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


namespace WebCraft {
	namespace Networking {
		namespace Sockets {
			// Typedefs
#ifdef _WIN32
// Windows
			typedef SOCKET SOCKET_HANDLE;
#else
// Linux
			typedef int SOCKET_HANDLE;
#endif

			// Declarations
			class SocketBase;
			class Socket;
			class ServerSocket;

			// Functions
			int SocketInit();
			int SocketCleanup();

		}
	}
}