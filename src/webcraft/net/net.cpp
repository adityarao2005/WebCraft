#include "webcraft/net/net.h"
#include <list>
namespace WebCraft {
	namespace Networking {
		// Empty handler
		Endpoint::ConnectionHandler Endpoint::DEFAULT_HANDLER = [](Connection& connection) {};

		// Socket stream buffer
		// Basically allows read and write through iostreams to a socket
		class SocketStreambuf : public std::streambuf {
		private:
			// Shared pointer of the socket
			std::shared_ptr<WebCraft::Networking::Sockets::Socket> socket;
			// buffer size
			static const int bufferSize = 256;
			// input and output buffers
			char inputBuffer[bufferSize];
			char outputBuffer[bufferSize];

		public:
			// Constructor
			SocketStreambuf(std::shared_ptr<WebCraft::Networking::Sockets::Socket> socket) : socket(socket) {
				setp(outputBuffer, outputBuffer + bufferSize - 1); // -1 to leave space for overflow '\n'
				setg(inputBuffer, inputBuffer, inputBuffer);
			}

		protected:
			// Called when output buffer is full (or in response to a call to 'pubsync()')
			int_type overflow(int_type ch) override {
				// If the character is not EOF, add it to the buffer
				if (ch != traits_type::eof()) {
					*pptr() = ch;
					pbump(1);
				}

				// Flush the buffer
				// If the character was EOF, return EOF
				if (flushBuffer() == EOF) {
					return traits_type::eof();
				}

				// Return the character
				return ch;
			}

			// Called when input buffer is empty
			int_type underflow() override {
				// If there is data in the buffer, return the next character
				if (gptr() < egptr()) {
					return traits_type::to_int_type(*gptr());
				}

				// Read data from the socket
				int numRead = socket->receive(inputBuffer, bufferSize);
				// If no data was read, return EOF
				if (numRead <= 0) {
					return traits_type::eof();
				}

				// Set the input buffer pointers
				setg(inputBuffer, inputBuffer, inputBuffer + numRead);

				// Return the first character
				return traits_type::to_int_type(*gptr());
			}

			// Called when the buffer is flushed
			int flushBuffer() {
				// Get the length of the buffer
				int len = int(pptr() - pbase());
				// If the length is 0, return 0
				if (socket->send(pbase(), len) != len) {
					return EOF;
				}
				// If the length is not 0, send the data
				pbump(-len);
				return len;
			}

			int sync() override {
				// Flush the buffer
				if (flushBuffer() == EOF) {
					return -1;
				}
				// Return 0
				return 0;
			}
		};

		// Server Constructor
		TCPServer::TCPServer() {
			// Default executor - Fixed thread pool executor with recommended threads
			executor = WebCraft::Util::Async::Executors::newFixedThreadPoolExecutor(std::thread::hardware_concurrency());
			// Default handler - Empty handler
			handler = Endpoint::DEFAULT_HANDLER;
		}

		TCPServer::~TCPServer() {
			// Shutdown the executor
			shutdown();
		}

		void TCPServer::setExecutor(std::unique_ptr<WebCraft::Util::Async::Executor> executor) {
			// Set the executor
			this->executor = std::move(executor);
		}

		void TCPServer::setConnectionHandler(Endpoint::ConnectionHandler handler) {
			// Set the handler
			this->handler = std::move(handler);
		}

		bool TCPServer::isRunning() {
			// Check if the executor is running
			if (executor == nullptr)
				return false;
			// Return the executor's running status
			return executor->isRunning();
		}

		void TCPServer::shutdown() {
			// Shutdown the executor
			executor->shutdown();
		}

		void TCPServer::start(int port) {
			// Bind the server socket
			server.bind(port);
			// Listen for connections
			server.listen();

			// While the server is running
			while (isRunning()) {
				// Accept a connection
				std::shared_ptr<WebCraft::Networking::Sockets::Socket> client = server.accept();
				// Execute the connection handler
				std::future<void> future = executor->execute(std::bind(&TCPServer::socket_handler, this, client, this->handler));
			}
		}

		// Connection handler
		void Endpoint::socket_handler(std::shared_ptr<WebCraft::Networking::Sockets::Socket> client, Endpoint::ConnectionHandler handler) {
			// Create connection
			Endpoint::Connection connection;
			// Declare request and response
			Endpoint::Request request;
			Endpoint::Response response;

			// Create input and output streams
			std::unique_ptr<std::istream> input;
			std::unique_ptr<std::ostream> output;

			// Create the input and output streams via streambuf
			std::unique_ptr<SocketStreambuf> streambuf = std::make_unique<SocketStreambuf>(client);
			input = std::make_unique<std::istream>(streambuf.get());
			output = std::make_unique<std::ostream>(streambuf.get());

			// Set input and output
			request.input = std::move(input);
			response.output = std::move(output);

			// Set the connection
			connection.request = std::move(request);
			connection.response = std::move(response);

			// Handle the connection
			handler(connection);
		}


		TCPClient::TCPClient() {}

		TCPClient::~TCPClient() {}

		void TCPClient::sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler) {
			// Create a new single thread executor
			std::shared_ptr<WebCraft::Util::Async::Executor> executor = WebCraft::Util::Async::Executors::newAsyncExecutor();
			// Send the request asynchronously
			sendAsync(uri, handler, executor);
		}

		void TCPClient::sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler, std::shared_ptr<WebCraft::Util::Async::Executor> executor) {
			// Create a new client socket
			std::shared_ptr<WebCraft::Networking::Sockets::Socket> client = std::make_shared<WebCraft::Networking::Sockets::Socket>();

			// Split the URI into host and port
			std::string host = uri.substr(0, uri.find(":"));
			std::string port = uri.substr(uri.find(":") + 1);
			// Connect to the host and port
			client->connect(host, std::stoi(port));

			// Execute the handler
			std::future<void> future = executor->execute(std::bind(&TCPClient::socket_handler, this, client, handler));
		}

		struct SocketConnection : public Endpoint::Connection {
			std::shared_ptr<WebCraft::Networking::Sockets::Socket> client;
			std::unique_ptr<SocketStreambuf> streambuf;

			SocketConnection(const std::string& uri) {
				// Create a new client socket
				client = std::make_shared<WebCraft::Networking::Sockets::Socket>();

				// Split the URI into host and port
				std::string host = uri.substr(0, uri.find(":"));
				std::string port = uri.substr(uri.find(":") + 1);
				// Connect to the host and port
				client->connect(host, std::stoi(port));

				// Create input and output streams
				std::unique_ptr<std::istream> input;
				std::unique_ptr<std::ostream> output;

				// Create the input and output streams via streambuf
				streambuf = std::make_unique<SocketStreambuf>(this->client);
				input = std::make_unique<std::istream>(streambuf.get());
				output = std::make_unique<std::ostream>(streambuf.get());

				// Set input and output
				request.input = std::move(input);
				response.output = std::move(output);
			}

			~SocketConnection() {
				// Close the socket
				Debug::log("asdjflaskdjflkdjsflkajsdfljasdf");
			}
		};

		std::shared_ptr<Endpoint::Connection> TCPClient::send(const std::string& uri) {
			// Return the connection
			return std::make_unique<SocketConnection>(uri);
		}
	}
}