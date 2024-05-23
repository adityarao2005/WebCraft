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
		// Set connection handler
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
		// Create a map to store the headers
		std::map<std::string, std::string> headers;
		// Read the headers
		std::string line;
		// Read until the end of the headers
		while (std::getline(stream, line) && line != "\r") {
			// Split the line into key and value
			std::istringstream is_line(line);
			std::string key;
			// Get the key
			if (std::getline(is_line, key, ':')) {
				// Get the value
				std::string value;
				// Get the value
				if (std::getline(is_line, value)) {
					headers[key] = value;
				}
			}
		}
		// Return the headers
		return headers;
	}

	void handler(WebCraft::Networking::Endpoint::Connection& connection) {
		// Get request and response objects
		WebCraft::Networking::Endpoint::Request& request = connection.request;
		WebCraft::Networking::Endpoint::Response& response = connection.response;

		// Read the request
		std::string path;
		std::getline(*request.input, path);
		Debug::log("Request: " + path);
		// Read the headers
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
		// Flush to output stream
		response.output->flush();

	}


	void test_server_shutdown() {
		// Get commands from the console
		std::string line;
		while (std::getline(std::cin, line)) {
			// If the command is shutdown, break
			if (line == "shutdown") {
				break;
			}
		}
		// Shutdown the server
		server.shutdown();
	}

	void test_client() {
		// Create a client
		WebCraft::Networking::Client client;
		// Connect to the server
		Debug::log("Test async no executor");
		client.sendAsync("google.com:80", [this](WebCraft::Networking::Endpoint::Connection& connection) {
			// Get request and response objects
			WebCraft::Networking::Endpoint::Request& request = connection.request;
			WebCraft::Networking::Endpoint::Response& response = connection.response;

			// Send the request
			*response.output << "GET / HTTP/1.1\r\n";
			*response.output << "Host: google.com\r\n";
			*response.output << "Connection: close\r\n";
			*response.output << "\r\n";
			// Flush to output stream
			response.output->flush();

			// Read the response
			std::string line;
			while (std::getline(*request.input, line)) {
				Debug::log(line);
			}

			});
		Debug::log("Test async with executor");
		client.sendAsync("google.com:80", [this](WebCraft::Networking::Endpoint::Connection& connection) {
			// Get request and response objects
			WebCraft::Networking::Endpoint::Request& request = connection.request;
			WebCraft::Networking::Endpoint::Response& response = connection.response;

			// Send the request
			*response.output << "GET / HTTP/1.1\r\n";
			*response.output << "Host: google.com\r\n";
			*response.output << "Connection: close\r\n";
			*response.output << "\r\n";
			// Flush to output stream
			response.output->flush();

			// Read the response
			std::string line;
			while (std::getline(*request.input, line)) {
				Debug::log(line);
			}

			}, Executors::newAsyncExecutor());

		Debug::log("Test sync");
		std::shared_ptr<WebCraft::Networking::Endpoint::Connection> connection = client.send("google.com:80");

		// Get request and response objects
		WebCraft::Networking::Endpoint::Request& request = connection->request;
		WebCraft::Networking::Endpoint::Response& response = connection->response;

		// Send the request
		*response.output << "GET / HTTP/1.1\r\n";
		*response.output << "Host: google.com\r\n";
		*response.output << "Connection: close\r\n";
		*response.output << "\r\n";
		// Flush to output stream
		response.output->flush();

		// Read the response
		std::string line;
		while (std::getline(*request.input, line)) {
			Debug::log(line);
		}
	}

	void Run() {
		//RUN_TEST_ASYNC(net_test, test_server);
		//RUN_TEST_ASYNC(net_test, test_server_shutdown);
		RUN_TEST_ASYNC(net_test, test_client);
	}
};