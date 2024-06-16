#pragma once
#include <webcraft/webcraft.h>
#include <webcraft/util/async.h>
#include <webcraft/util/debug.h>
#include <webcraft/util/io.h>
#include <sstream>

namespace WebCraft {
	namespace Net {

		// Define the interfaces
		struct Endpoint;
		struct Server;
		struct Client;

		// Connection type
		class Connection {
		private:
			// The stream
			std::shared_ptr<WebCraft::Util::IO::Async::DuplexStream> stream;
			std::string address;

		public:
			// Constructor
			Connection() {}
			// Constructor
			Connection(WebCraft::Util::IO::Async::DuplexStream* stream) : stream(stream) {}

			// Destructor
			std::shared_ptr<WebCraft::Util::IO::Async::DuplexStream> getStream() {
				return stream;
			}

			std::string getAddress() {
				return address;
			}
		};

		struct Server {
			Server() = default;
			~Server() = default;

			virtual async(void) start(int port) = 0;
			virtual async(void) stop() = 0;
			virtual async(void) onconnect(std::unique_ptr<Connection> connection);
		};

		struct Client {
			Client() = default;
			~Client() = default;

			virtual async(std::unique_ptr<Connection>) connect(std::string address, int port) = 0;
		};

		namespace Tcp {
			class Server : public WebCraft::Net::Server {
			private:
			public:
				// The server
				Server() = default;
				~Server() = default;

				virtual async(void) start(int port);
				virtual async(void) stop();
				virtual async(void) onconnect(std::unique_ptr<Connection> connection) override;
			};

			class Client : public WebCraft::Net::Client {
			private:
			public:
				// The client
				Client() = default;
				~Client() = default;

				virtual async(std::unique_ptr<Connection>) connect(std::string address, int port) override;
			};
		}

		namespace Udp {
			class Server : public WebCraft::Net::Server {
			private:
			public:
				// The server
				Server() = default;
				~Server() = default;

				virtual async(void) start(int port);
				virtual async(void) stop();
				virtual async(void) onconnect(std::unique_ptr<Connection> connection);
			};

			class Client : public WebCraft::Net::Client {
			private:
			public:
				// The client
				Client() = default;
				~Client() = default;

				virtual async(std::unique_ptr<Connection>) connect(std::string address, int port);
			};
		}

	}
}