#pragma once

#include "webcraft/webcraft.h"
#include "webcraft/net/net.h"

#include <map>
#include <sstream>
#include "webcraft/util/debug.h"
#include "webcraft/util/unit_test.h"
#include "webcraft/util/async.h"

using namespace WebCraft::Util;
using namespace WebCraft::Util::Async;

class net_test : public WebCraft::Util::UnitTest {
private:
	WebCraft::Networking::Server server;

public:
	// To string method
	TO_STRING(net_test);

	// Constructor
	net_test() {
		server.setConnectionHandler([this](WebCraft::Networking::Endpoint::Connection& conn) {
			handler(conn);
			});
	}


	void onTestFailed(const std::string& name, const std::exception& e) override {
		UnitTest::onTestFailed(name, e);
		Debug::log(std::string(e.what()), WebCraft::Util::LogLevel::ERROR);
	}

	void test_server() {
		server.start(8080);
	}

	std::map<std::string, std::string> readHeaders(std::istream& stream) {
		std::map<std::string, std::string> headers;
		std::string line;
		while (std::getline(stream, line) && line != "\r") {
			std::istringstream is_line(line);
			std::string key;
			if (std::getline(is_line, key, ':')) {
				std::string value;
				if (std::getline(is_line, value)) {
					headers[key] = value;
				}
			}
		}
		return headers;
	}

	void handler(WebCraft::Networking::Endpoint::Connection& connection) {

		WebCraft::Networking::Endpoint::Request& request = connection.request;
		WebCraft::Networking::Endpoint::Response& response = connection.response;

		// Read the request
		std::map<std::string, std::string> headers = readHeaders(*request.input);
		for (auto const& x : headers) {
			Debug::log(x.first + ": " + x.second);
		}

		// Write the response
		*response.output << "HTTP/1.1 200 OK\r\n";
		*response.output << "Content-Type: text/html\r\n";
		*response.output << "Content-Length: 11\r\n";
		*response.output << "Connection: close\r\n";
		*response.output << "\r\n";
		*response.output << "Hello World";
		response.output->flush();

	}


	void test_server_shutdown() {
		std::string line;
		while (std::getline(std::cin, line)) {
			if (line == "shutdown") {
				break;
			}
		}
		server.shutdown();
	}

	void Run() {
		RUN_TEST_ASYNC(net_test, test_server);
		RUN_TEST_ASYNC(net_test, test_server_shutdown);
	}
};