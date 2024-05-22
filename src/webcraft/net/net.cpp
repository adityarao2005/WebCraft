#include "webcraft/net/net.h"
#include <list>
namespace WebCraft {
	namespace Networking {
		Endpoint::ConnectionHandler Endpoint::DEFAULT_HANDLER = [](Connection& connection) {};

		class SocketStreambuf : public std::streambuf {
		private:
			std::shared_ptr<WebCraft::Networking::Sockets::Socket> socket;
			static const int bufferSize = 256;
			char inputBuffer[bufferSize];
			char outputBuffer[bufferSize];

		public:
			SocketStreambuf(std::shared_ptr<WebCraft::Networking::Sockets::Socket> socket) : socket(socket) {
				setp(outputBuffer, outputBuffer + bufferSize - 1); // -1 to leave space for overflow '\n'
				setg(inputBuffer, inputBuffer, inputBuffer);
			}

		protected:
			// Called when output buffer is full (or in response to a call to 'pubsync()')
			int_type overflow(int_type ch) override {
				if (ch != traits_type::eof()) {
					*pptr() = ch;
					pbump(1);
				}

				if (flushBuffer() == EOF) {
					return traits_type::eof();
				}

				return ch;
			}

			// Called when input buffer is empty
			int_type underflow() override {
				if (gptr() < egptr()) {
					return traits_type::to_int_type(*gptr());
				}

				int numRead = socket->receive(inputBuffer, bufferSize);
				if (numRead <= 0) {
					return traits_type::eof();
				}

				setg(inputBuffer, inputBuffer, inputBuffer + numRead);

				return traits_type::to_int_type(*gptr());
			}

			int flushBuffer() {
				int len = int(pptr() - pbase());
				if (socket->send(pbase(), len) != len) {
					return EOF;
				}
				pbump(-len);
				return len;
			}

			int sync() override {
				if (flushBuffer() == EOF) {
					return -1;
				}
				return 0;
			}
		};


		Server::Server() {
			executor = WebCraft::Util::Async::Executors::newFixedThreadPoolExecutor(std::thread::hardware_concurrency());
			handler = Endpoint::DEFAULT_HANDLER;
		}

		Server::~Server() {
			shutdown();
		}

		void Server::setExecutor(std::unique_ptr<WebCraft::Util::Async::Executor> executor) {
			this->executor = std::move(executor);
		}

		void Server::setConnectionHandler(Endpoint::ConnectionHandler handler) {
			this->handler = std::move(handler);
		}

		bool Server::isRunning() {
			if (executor == nullptr)
				return false;
			return executor->isRunning();
		}

		void Server::shutdown() {
			executor->shutdown();
		}




		void Server::start(int port) {
			server.bind(port);
			server.listen();

			while (isRunning()) {
				std::shared_ptr<WebCraft::Networking::Sockets::Socket> client = server.accept();
				std::future<void> future = executor->execute(std::bind(&Server::socket_handler, this, client));
			}
		}

		void Server::socket_handler(std::shared_ptr< WebCraft::Networking::Sockets::Socket> client) {
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


		Client::Client() {}

		Client::~Client() {}

		void Client::sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler) {
			WebCraft::Networking::Sockets::Socket socket;
			//socket.connect(uri);
		}

		void Client::sendAsync(const std::string& uri, Endpoint::ConnectionHandler handler, std::shared_ptr<WebCraft::Util::Async::Executor> executor) {
			WebCraft::Networking::Sockets::Socket socket;
			//socket.connect(uri);
			//socket.send(data.c_str(), data.length());
		}

		Endpoint::Connection& Client::send(const std::string& uri) {
			WebCraft::Networking::Sockets::Socket socket;
			//socket.connect(uri);
			//socket.send(data.c_str(), data.length());
			//return connection;
			Connection connection;
			return connection;
		}
	}
}