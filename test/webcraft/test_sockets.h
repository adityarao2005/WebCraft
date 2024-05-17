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
#define DEFAULT_PORT 27015

class sockets_test : public WebCraft::Util::UnitTest {
public:
	// To string method
	TO_STRING(sockets_test);

	/// <summary>
	/// When test fails, this method is called
	/// </summary>
	/// <param name="message"></param>
	/// <param name="e"></param>
	void onTestFailed(const std::string& message, std::exception e) override {
		UnitTest::onTestFailed(message, e);
		Debug::log("Reason: " + std::string(e.what()));
	}

	/// <summary>
	/// Client socket test communication with server
	/// This method is asynchronously called
	/// </summary>
	void test_client() {
		// Sleep for 1 second
		std::this_thread::sleep_for(std::chrono::seconds(1));
		// Check if async is working based on thread id
		std::stringstream ss;
		ss << "This thread: " << std::this_thread::get_id();
		Debug::log(ss.str());

		// creating socket 
		WebCraft::Networking::Sockets::Socket socket;

		// Connect to server.
		socket.connect("localhost", DEFAULT_PORT);

		// Send an initial buffer
		const char* sendbuf = "this is a test";
		int send_len = socket.send(sendbuf, (int)strlen(sendbuf));
		// log the bytes sent
		Debug::log("Bytes Sent: " + std::to_string(send_len));

		// Shut down write
		socket.shutdown();

		int iResult;
		char recvbuf[DEFAULT_BUFLEN];
		int recvbuflen = DEFAULT_BUFLEN;
		// Receive until the peer closes the connection
		do {
			iResult = socket.receive(recvbuf, recvbuflen);
			if (iResult == 0)
				Debug::log("Connection closed");
			else
				Debug::log("Bytes received: " + std::to_string(iResult));
		} while (iResult > 0);

	}

	void test_server() {
		// Check if async is working based on thread id
		std::stringstream ss;
		ss << "This thread: " << std::this_thread::get_id();
		Debug::log(ss.str());

		

		// Create a server socket
		WebCraft::Networking::Sockets::ServerSocket server;
		// Bind the server to the default port
		server.bind(DEFAULT_PORT);
		// Listen for incoming connections
		server.listen();

		// Accept a client socket
		WebCraft::Networking::Sockets::Socket peer = server.accept();

		int iResult;
		char recvbuf[DEFAULT_BUFLEN];
		int recvbuflen = DEFAULT_BUFLEN;
		// Receive until the peer shuts down the connection
		do {

			iResult = peer.receive(recvbuf, recvbuflen);

			if (iResult == 0)
				Debug::log("Connection closing...\n");
			else {
				Debug::log("Bytes received: " + std::to_string(iResult));

				// Echo the buffer back to the sender
				int iSendResult = peer.send(recvbuf, iResult);
				Debug::log("Bytes sent: " + iSendResult);
			}

		} while (iResult > 0);

		// shutdown the connection since we're done
		peer.shutdown();
	}

	/// <summary>
	/// Run the test
	/// </summary>
	void Run() {
		RUN_TEST_ASYNC(sockets_test, test_server);
		RUN_TEST_ASYNC(sockets_test, test_client);

	}

};