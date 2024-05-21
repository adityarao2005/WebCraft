#pragma once
#include "webcraft/webcraft.h"
#include "webcraft/net/sockets.h"
#include "webcraft/util/async.h"

namespace WebCraft {
	namespace Networking {

		// Forward declare the Endpoint class
		class Endpoint;
		// Declare the constructs for the Server and Client classes
		class Server;
		class Client;

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
			};

			/// <summary>
			/// Represents a handler for the server
			/// </summary>
			using ConnectionHandler = std::function<void(Connection&)>;
			static ConnectionHandler DEFAULT_HANDLER;
		};

		/// <summary>
		/// Represents a server
		/// </summary>
		class Server {
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

			/// <summary>
			/// Indicates if the server is stopped
			/// </summary>
			bool stop = false;
			std::mutex stopMutex;

		public:
			/// <summary>
			/// Constructor for the server
			/// </summary>
			Server();
			/// <summary>
			/// Destructor for the server
			/// </summary>
			~Server();

			/// <summary>
			/// Sets the executor for the server
			/// </summary>
			void setExecutor(std::unique_ptr<WebCraft::Util::Async::Executor> executor);

			/// <summary>
			/// Starts the server on the specified port
			/// </summary>
			void start(int port);

			/// <summary>
			/// Shuts down the server
			/// </summary>
			void shutdown();

			/// <summary>
			/// Sets the connection handler for the server
			/// </summary>
			void setConnectionHandler(Endpoint::ConnectionHandler handler);

			/// <summary>
			/// Indicates if the server is stopped
			/// </summary>
			bool isStopped();
		};

		/// <summary>
		/// Represents a client
		/// </summary>
		class Client : public Endpoint {
		public:
			/// <summary>
			/// Constructor for the client
			/// </summary>
			Client();
			/// <summary>
			/// Destructor for the client
			/// </summary>
			~Client();

			/// <summary>
			/// Connects to the specified URI
			/// </summary>
			void sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler);

			/// <summary>
			/// Connects to the specified URI
			/// </summary>
			void sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler, std::shared_ptr<WebCraft::Util::Async::Executor> executor);

			/// <summary>
			/// Connects to the specified URI
			/// </summary>
			Endpoint::Connection& send(const std::string& uri);
		};


	}
}