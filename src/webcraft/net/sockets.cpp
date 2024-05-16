
#include "webcraft/net/sockets.h"

using namespace WebCraft::Networking::Sockets;

// Initialize the socket library
int WebCraft::Networking::Sockets::SocketInit() {
	// If we're on Windows, we need to initialize the Winsock library
#ifdef _WIN32
	WSADATA wsa_data;
	return WSAStartup(MAKEWORD(1, 1), &wsa_data);
#else
	// On non-Windows platforms, we don't need to do anything
	return 0;
#endif
}

int WebCraft::Networking::Sockets::SocketCleanup() {
	// If we're on Windows, we need to clean up the Winsock library
#ifdef _WIN32
	return WSACleanup();
#else
	// On non-Windows platforms, we don't need to do anything
	return 0;
#endif
}
