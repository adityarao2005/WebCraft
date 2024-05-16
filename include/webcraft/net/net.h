#pragma once
#include "webcraft/webcraft.h"
#include "webcraft/net/socket.h"

namespace WebCraft {
	namespace Networking {
		using namespace WebCraft::Networking::Sockets;

		// Forward declare the Endpoint class
		class Endpoint;
		// Declare the constructs for the Server and Client classes
		class Server;
		class Client;
		// Declare the Request and Response structs
		struct request;
		struct response;

		/// <summary>
		/// Represents a request to the server
		/// </summary>
		struct request {
			/// <summary>
			/// Input stream for the request
			/// </summary>
			std::unique_ptr<std::istream> input;
		};

		/// <summary>
		/// Represents a response from the server
		/// </summary>
		struct response {
			/// <summary>
			/// Output stream for the response
			/// </summary>
			std::unique_ptr<std::ostream> ostream;
		};

		/// <summary>
		/// Represents a handler for the server
		/// </summary>
		using Handler = std::function<void(request&, response&)>;

		/// <summary>
		/// Represents an endpoint for the server or client
		/// </summary>
		class Endpoint {
		public:
			/// <summary>
			/// On connect handler
			/// </summary>
			Handler onconnect;
		};

		/// <summary>
		/// Represents a server
		/// </summary>
		class Server : public Endpoint {
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
			/// Starts the server on the specified port
			/// </summary>
			void start(int port);

			/// <summary>
			/// Shuts down the server
			/// </summary>
			void shutdown();
		private:
			/// <summary>
			/// Server socket
			/// </summary>
			ServerSocket server;
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
			void send(const std::string& uri);
		private:
			/// <summary>
			/// Client socket
			/// </summary>
			Socket socket;
		};


	}
}