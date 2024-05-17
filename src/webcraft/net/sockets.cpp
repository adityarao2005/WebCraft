
#include "webcraft/net/sockets.h"

using namespace WebCraft::Networking::Sockets;

// For linux compatibility
#ifndef _WIN32
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define SD_SEND SHUT_WR
#endif

// For windows compatibility
#ifdef _WIN32
// RAII for handling WSAStartup and WSACleanup
class SocketEssentials {
public:
	// Initialize Winsock
	SocketEssentials() {
		WSADATA wsa_data;
		int result = WSAStartup(MAKEWORD(1, 1), &wsa_data);
		if (result != 0) {
			Debug::throwException("WSAStartup failed with error: " + std::to_string(result));
		}
	}

	// Cleanup winsock
	~SocketEssentials() {
		WSACleanup();
	}
};

// Initialize the essentials
SocketEssentials essentials;
#endif

// SocketBase methods

SocketBase::SocketBase() {
	// Create a SOCKET for connecting to server
	if ((handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		Debug::throwException("socket failed with error");
	}
}

SocketBase::~SocketBase() {
	closesocket(handle);
}

void SocketBase::create_address(const char* host, int port, addrinfo** result, bool server) {
	// Setup the hints address info structure
	addrinfo hints = {0};

	// Set the address family, socket type, and protocol
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	// If server, set the AI_PASSIVE flag
	if (server) {
		hints.ai_flags = AI_PASSIVE;
	}


	// Resolve the server address and port
	int iResult;
	if ((iResult = getaddrinfo(host, std::to_string(port).data(), &hints, result)) != 0) {
		Debug::throwException("getaddrinfo failed with error: " + std::to_string(iResult));
	}

}

// Socket methods

void Socket::connect(std::string host, int port) {
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

int Socket::send(const char* data, int length) {
	int iResult;
	// Send an initial buffer
	if ((iResult = ::send(handle, data, length, 0)) == SOCKET_ERROR) {
		Debug::throwException("send failed with error");
	}
	return iResult;
}

int Socket::receive(char* data, int length) {
	int iResult;
	// Receive into buffer with buffer length
	if ((iResult = ::recv(handle, data, length, 0)) == SOCKET_ERROR) {
		Debug::throwException("recv failed with error");
	}
	return iResult;
}

void Socket::shutdown() {
	// shutdown the connection since no more data will be sent
	if (::shutdown(handle, SD_SEND) == SOCKET_ERROR) {
		Debug::throwException("shutdown failed with error");
	}
}

void ServerSocket::bind(int port) {
	addrinfo* result = nullptr;
	create_address(nullptr, port, &result, true);

	// Setup the TCP listening socket
	if (::bind(handle, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
		Debug::throwException("bind failed with error");
	}

	// no longer need address info for server
	freeaddrinfo(result);
}

void ServerSocket::listen() {
	// Listen on the socket
	if (::listen(handle, SOMAXCONN) == SOCKET_ERROR) {
		Debug::throwException("listen failed with error");
	}
}

Socket ServerSocket::accept() {
	// Accept a client socket
	SOCKET client_socket = ::accept(handle, nullptr, nullptr);
	
	// if accept failed
	if (client_socket == INVALID_SOCKET) {
		Debug::throwException("accept failed with error");
	}
	// return the client socket
	return Socket(client_socket);
}