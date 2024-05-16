#pragma once
#include <iostream>
#include <thread>
#include <sstream>
#include <string>
#include <utility>

#include "webcraft/util/debug.h"
#include "webcraft/util/unit_test.h"
#include "webcraft/net/sockets.h"

using namespace WebCraft::Util;

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

#ifdef _WIN32
// Windows
typedef SOCKET SOCKET_HANDLE;
#else
// Linux
typedef int SOCKET_HANDLE;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define SD_SEND SHUT_WR
#endif

class sockets_test : public WebCraft::Util::UnitTest {
public:

	TO_STRING(sockets_test);

	sockets_test() {
		// initializing the socket library
		WebCraft::Networking::Sockets::SocketInit();
		Debug::log("Socket library initialized");
	}

	~sockets_test() {
		// cleaning up the socket library
		WebCraft::Networking::Sockets::SocketCleanup();
		Debug::log("Socket library cleaned up");
	}

	void onTestFailed(const std::string& message, std::exception e) override {
		UnitTest::onTestFailed(message, e);
		Debug::log("Reason: " + std::string(e.what()));
	}

	void test_client() {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::stringstream ss;
		ss << "This thread: " << std::this_thread::get_id();
		Debug::log(ss.str());

		// creating socket 
		SOCKET_HANDLE ConnectSocket = INVALID_SOCKET;
		struct addrinfo* result = NULL,
			* ptr = NULL,
			hints;
		const char* sendbuf = "this is a test";
		char recvbuf[DEFAULT_BUFLEN];
		int recvbuflen = DEFAULT_BUFLEN;


		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		// Resolve the server address and 
		int iResult;
		if ((iResult = getaddrinfo("localhost", DEFAULT_PORT, &hints, &result)) != 0) {
			Debug::throwException("getaddrinfo failed with error: " + std::to_string(iResult));
		}

		// Attempt to connect to an address until one succeeds
		for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

			// Create a SOCKET_HANDLE for connecting to server
			ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol);
			if (ConnectSocket == INVALID_SOCKET) {
				Debug::throwException("socket failed with error");
			}

			// Connect to server.
			if (connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
				closesocket(ConnectSocket);
				ConnectSocket = INVALID_SOCKET;
				continue;
			}
			break;
		}

		// no longer need address info for server
		freeaddrinfo(result);

		// if connection failed
		if (ConnectSocket == INVALID_SOCKET) {
			Debug::throwException("Unable to connect to server!");
		}

		// Send an initial buffer
		if (send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0) == SOCKET_ERROR) {
			Debug::throwException("send failed with error");
		}

		// log the bytes sent
		Debug::log("Bytes Sent: " + std::to_string(iResult));

		// shutdown the connection since no more data will be sent
		if (shutdown(ConnectSocket, SD_SEND) == SOCKET_ERROR) {
			Debug::throwException("shutdown failed with error");
		}

		// Receive until the peer closes the connection
		do {
			// receive data
			iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
			
			if (iResult > 0)
				Debug::log("Bytes received: " + std::to_string(iResult));
			else if (iResult == 0)
				Debug::log("Connection closed");
			else
				Debug::throwException("recv failed with error");

		} while (iResult > 0);

		// cleanup
		closesocket(ConnectSocket);

	}

	void test_server() {
		std::stringstream ss;
		ss << "This thread: " << std::this_thread::get_id();
		Debug::log(ss.str());

		// creating socket 
		int iResult;

		SOCKET_HANDLE ListenSocket = INVALID_SOCKET;
		SOCKET_HANDLE ClientSocket = INVALID_SOCKET;

		struct addrinfo* result = NULL;
		struct addrinfo hints;

		int iSendResult;
		char recvbuf[DEFAULT_BUFLEN];
		int recvbuflen = DEFAULT_BUFLEN;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		// Resolve the server address and port
		iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
		if (iResult != 0) {
			Debug::throwException("getaddrinfo failed with error: "+ iResult);
		}

		// Create a SOCKET_HANDLE for the server to listen for client connections.
		ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {
			Debug::throwException("socket failed with error");
		}

		// Setup the TCP listening socket
		iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			Debug::throwException("bind failed with error");
		}

		freeaddrinfo(result);

		iResult = listen(ListenSocket, SOMAXCONN);
		if (iResult == SOCKET_ERROR) {
			Debug::throwException("listen failed with error");
		}

		// Accept a client socket
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			Debug::throwException("accept failed with error");
		}

		// No longer need server socket
		closesocket(ListenSocket);

		// Receive until the peer shuts down the connection
		do {

			iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0) {
				Debug::log("Bytes received: " + std::to_string(iResult));

				// Echo the buffer back to the sender
				iSendResult = send(ClientSocket, recvbuf, iResult, 0);
				if (iSendResult == SOCKET_ERROR) {
					Debug::throwException("send failed with error");
				}
				Debug::log("Bytes sent: " + iSendResult);
			}
			else if (iResult == 0)
				Debug::log("Connection closing...\n");
			else {
				Debug::throwException("recv failed with error");
			}

		} while (iResult > 0);

		// shutdown the connection since we're done
		iResult = shutdown(ClientSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			Debug::throwException("shutdown failed with error");
		}

		// cleanup
		closesocket(ClientSocket);
	}

	void Run() {
		RUN_TEST_ASYNC(sockets_test, test_server);
		RUN_TEST_ASYNC(sockets_test, test_client);

	}

};