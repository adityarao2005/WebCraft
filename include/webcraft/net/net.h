#pragma once
#include "webcraft/webcraft.h"
#include "webcraft/net/sockets.h"
#include "webcraft/util/async.h"
#include "webcraft/util/debug.h"

namespace WebCraft {
	namespace Networking {

		// Forward declare the Endpoint class
		class Endpoint;
		// Server and client classes
		class Server;
		class Client;
		// Declare the constructs for the TCP Server and Client classes
		class TCPServer;
		class TCPClient;

		/// <summary>
		/// Represents an endpoint for the server or client
		/// </summary>
		class Endpoint {
		public:

			/// <summary>
			/// Represents a request to the server
			/// </summary>
			struct Request {
				/// <summary>
				/// Input stream for the request
				/// </summary>
				std::unique_ptr<std::istream> input;
			};

			/// <summary>
			/// Represents a response from the server
			/// </summary>
			struct Response {
				/// <summary>
				/// Output stream for the response
				/// </summary>
				std::unique_ptr<std::ostream> output;
			};


			/// <summary>
			/// Represents a connection to the server
			/// </summary>
			struct Connection {
				/// <summary>
				/// Input stream for the connection
				///	</summary>
				Request request;
				/// <summary>
				/// Output stream for the connection
				/// </summary>
				Response response;

				/// <summary>
				/// Destructor for the connection
				/// </summary>
				Connection() {}

				~Connection() {
					Debug::log("Connection destroyed");
				}
			};

			/// <summary>
			/// Represents a handler for the server
			/// </summary>
			using ConnectionHandler = std::function<void(Connection&)>;
			/// <summary>
			/// Represents a handler for the client
			/// </summary>
			static ConnectionHandler DEFAULT_HANDLER;

			/// <summary>
			/// Tells what to do when the socket is connected
			/// </summary>
			void socket_handler(std::shared_ptr<WebCraft::Networking::Sockets::Socket> client, Endpoint::ConnectionHandler handler);
		};

		class Server : public Endpoint {
		public:
			/// <summary>
			/// Constructor for the server
			/// </summary>
			Server() {}

			/// <summary>
			/// Destructor for the server
			/// </summary>
			~Server() {}

			/// <summary>
			/// Starts the server on the specified port
			/// </summary>
			virtual void start(int port) = 0;

			/// <summary>
			/// Shuts down the server
			/// </summary>
			virtual void shutdown() = 0;

			/// <summary>
			/// Sets the connection handler for the server
			/// </summary>
			virtual void setConnectionHandler(Endpoint::ConnectionHandler handler) = 0;

			/// <summary>
			/// Sets the executor for the server
			/// </summary>
			virtual void setExecutor(std::unique_ptr<WebCraft::Util::Async::Executor> executor) = 0;

			/// <summary>
			/// Indicates if the server is stopped
			/// </summary>
			virtual bool isRunning() = 0;
		};

		/// <summary>
		/// Represents a client
		/// </summary>
		class Client : public Endpoint {
		public:
			/// <summary>
			/// Constructor for the client
			/// </summary>
			Client() {}

			/// <summary>
			/// Destructor for the client
			/// </summary>
			~Client() {}

			/// <summary>
			/// Connects to the specified URI
			/// </summary>
			virtual void sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler) = 0;

			/// <summary>
			/// Connects to the specified URI
			/// </summary>
			virtual void sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler, std::shared_ptr<WebCraft::Util::Async::Executor> executor) = 0;

			/// <summary>
			/// Connects to the specified URI
			/// </summary>
			virtual std::shared_ptr<Endpoint::Connection> send(const std::string& uri) = 0;
		};

		/// <summary>
		/// Represents a server
		/// </summary>
		class TCPServer : public Server {
		private:
			/// <summary>
			/// Server socket
			/// </summary>
			WebCraft::Networking::Sockets::ServerSocket server;

			/// <summary>
			/// Server thread
			/// </summary>
			std::unique_ptr<WebCraft::Util::Async::Executor> executor;

			/// <summary>
			/// Connection handler
			/// </summary>
			Endpoint::ConnectionHandler handler;

		public:
			/// <summary>
			/// Constructor for the server
			/// </summary>
			TCPServer();
			/// <summary>
			/// Destructor for the server
			/// </summary>
			~TCPServer();

			/// <summary>
			/// Sets the executor for the server
			/// </summary>
			void setExecutor(std::unique_ptr<WebCraft::Util::Async::Executor> executor) override;

			/// <summary>
			/// Starts the server on the specified port
			/// </summary>
			void start(int port) override;

			/// <summary>
			/// Shuts down the server
			/// </summary>
			void shutdown() override;

			/// <summary>
			/// Sets the connection handler for the server
			/// </summary>
			void setConnectionHandler(Endpoint::ConnectionHandler handler) override;

			/// <summary>
			/// Indicates if the server is stopped
			/// </summary>
			bool isRunning() override;
		};

		/// <summary>
		/// Represents a client
		/// </summary>
		class TCPClient : public Client {
		public:
			/// <summary>
			/// Constructor for the client
			/// </summary>
			TCPClient();
			/// <summary>
			/// Destructor for the client
			/// </summary>
			~TCPClient();

			/// <summary>
			/// Connects to the specified URI
			/// </summary>
			void sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler) override;

			/// <summary>
			/// Connects to the specified URI
			/// </summary>
			void sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler, std::shared_ptr<WebCraft::Util::Async::Executor> executor) override;

			/// <summary>
			/// Connects to the specified URI
			/// </summary>
			std::shared_ptr<Endpoint::Connection> send(const std::string& uri) override;
		};


	}
}