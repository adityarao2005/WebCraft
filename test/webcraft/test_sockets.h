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

	void onTestPassed(const std::string& name) override {
		Debug::log("Test passed: " + name + " ---------------------------------------");
	}


	void test_client() {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::stringstream ss;
		ss << "This thread: " << std::this_thread::get_id();
		Debug::log(ss.str());

		// creating socket 
		WebCraft::Networking::Sockets::Socket socket;
		const char* sendbuf = "this is a test";
		char recvbuf[DEFAULT_BUFLEN];
		int recvbuflen = DEFAULT_BUFLEN;

		socket.connect("localhost", DEFAULT_PORT);

		// Send an initial buffer
		int send_len = socket.send(sendbuf, (int)strlen(sendbuf));
		// log the bytes sent
		Debug::log("Bytes Sent: " + std::to_string(send_len));

		// Shut down write
		socket.shutdown();

		// Receive until the peer closes the connection
		int iResult;
		do {
			iResult = socket.receive(recvbuf, recvbuflen);
			if (iResult == 0)
				Debug::log("Connection closed");
			else
				Debug::log("Bytes received: " + std::to_string(iResult));
		} while (iResult > 0);

	}

	void test_server() {
		std::stringstream ss;
		ss << "This thread: " << std::this_thread::get_id();
		Debug::log(ss.str());


		int iSendResult;
		char recvbuf[DEFAULT_BUFLEN];
		int recvbuflen = DEFAULT_BUFLEN;

		WebCraft::Networking::Sockets::ServerSocket server;
		server.bind(DEFAULT_PORT);
		server.listen();

		WebCraft::Networking::Sockets::Socket peer = server.accept();

		// Receive until the peer shuts down the connection
		int iResult;
		do {

			iResult = peer.receive(recvbuf, recvbuflen);

			if (iResult == 0)
				Debug::log("Connection closing...\n");
			else {
				Debug::log("Bytes received: " + std::to_string(iResult));

				// Echo the buffer back to the sender
				iSendResult = peer.send(recvbuf, iResult);
				Debug::log("Bytes sent: " + iSendResult);
			}

		} while (iResult > 0);

		peer.shutdown();
	}

	void Run() {
		RUN_TEST_ASYNC(sockets_test, test_server);
		RUN_TEST_ASYNC(sockets_test, test_client);

	}

};