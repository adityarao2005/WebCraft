#include <webcraft/net/net.h>
#include <cstddef>

namespace WebCraft {
	namespace Net {
		namespace Http {
			class HttpClientBase;
			class HttpServerBase;
			class HttpConnection;

			class HttpClient;
			class HttpServer;

			enum ConnectionMethod {
				UDP, TCP
			};

			class HttpClientBase : public Net::Client {
			private:
				std::unique_ptr<Net::Client> client_impl;
			public:
				HttpClientBase(ConnectionMethod method = TCP);
				~HttpClientBase();

				struct Request {
					std::unique_ptr<WebCraft::Util::IO::Async::WritableStream> output;
					std::unordered_map<std::string, std::string> headers;
					std::string method;
					std::string path;
					std::string version;
				};

				struct Response {
					std::unique_ptr<WebCraft::Util::IO::Async::ReadableStream> input;
					std::unordered_map<std::string, std::string> headers;
					int status;
				};

				struct HttpConnection {
					std::unique_ptr<Request> request;
					std::unique_ptr<Response> response;
				};

				async(std::unique_ptr<Net::Connection>) connect(std::string address, int port) override;
				virtual async(std::unique_ptr<HttpConnection>) sendRequest(std::string uri);
			};

			class HttpServerBase : public Net::Server {
			private:
				std::unique_ptr<Net::Server> server_impl;
			public:
				HttpServerBase(ConnectionMethod method = TCP);
				~HttpServerBase();

				struct Request {
					std::unique_ptr<WebCraft::Util::IO::Async::ReadableStream> input;
					std::unordered_map<std::string, std::string> headers;
					std::string method;
					std::string path;
					std::string version;
				};

				struct Response {
					std::unique_ptr<WebCraft::Util::IO::Async::WritableStream> output;
					std::unordered_map<std::string, std::string> headers;
					int status;
				};

				struct HttpConnection {
					std::unique_ptr<Request> request;
					std::unique_ptr<Response> response;
				};

				async(void) bind(int port);
				async(void) start();
				async(void) stop();
				async(void) onconnect(std::unique_ptr<Connection> connection) override;
				virtual async(void) accept(std::unique_ptr<HttpConnection> connection) = 0;
			};

			template<typename T>
			struct BodyHandler;

			template<class T>
			struct BodyHandler {
				using ByteChunk = std::pair<std::byte, size_t>;
				using BodySubscriber = WebCraft::Util::Async::Generator<ByteChunk>;

				virtual std::unique_ptr<T> getBody(BodySubscriber buffers) {
					return nullptr;
				}


			};

			class BodyHandlers {
			public:
				template<typename T>
				using Generator = WebCraft::Util::Async::Generator<T>;

				static std::unique_ptr<BodyHandler<std::string>> string();
				static std::unique_ptr<BodyHandler<std::vector<std::byte>>> bytes();
				static std::unique_ptr<BodyHandler<WebCraft::Util::IO::Async::ReadableStream>> stream();
				static std::unique_ptr<BodyHandler<std::vector<std::string>>> lines();
				static std::unique_ptr<BodyHandler<Generator<std::string>>> linesGenerator();
				static std::unique_ptr<BodyHandler<std::vector<std::string>>> separatedBy(std::string value);
				static std::unique_ptr<BodyHandler<Generator<std::string>>> separatedByGenerator(std::string value);
				static std::unique_ptr<BodyHandler<void>> none();
			};


		}

	}
}