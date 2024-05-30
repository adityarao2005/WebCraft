#include "webcraft/net/net.h"

namespace WebCraft {
	namespace Http {

		// Forward declarations
		class HttpServer;
		class HttpClient;

		// Http Request and Response classes
		class HttpRequest;
		class HttpResponse;
		class HttpRequestBuilder;
		class HttpResponseBuilder;

		// Body subscriber and publisher classes
		template<typename T>
		class BodySubscriber;
		template<typename T>
		class BodyPublisher;

		// Http controller and router classes
		class Controller;
		class Router;

		// Http utils class
		class HttpUtils;

		// Http server and client classes
		class HttpClient {
			const HttpResponse send(const HttpRequest& request);
			
		};

		// Http Implementation
		// Http request class
		class HttpRequest {
		public:
			HttpRequest();
			~HttpRequest();

			// Getters
			std::string GetMethod() const;
			std::string GetPath() const;
			std::string GetVersion() const;
			std::string GetHeader(const std::string& key) const;
			std::unordered_map<std::string, std::string> GetHeaders() const;
			// Gets the body as a body subscriber
			template<typename T>
			BodySubscriber<T> GetBody() const;

			// Setters
			void SetMethod(const std::string& method);
			void SetPath(const std::string& path);
			void SetVersion(const std::string& version);
			void SetHeader(const std::string& key, const std::string& value);
			void SetHeaders(const std::unordered_map<std::string, std::string>& headers);
			// Sets the body as a body publisher
			template<typename T>
			void SetBody(const BodyPublisher<T>& body);
		};

		// Http response class
		class HttpResponse {
		public:
			HttpResponse();
			~HttpResponse();

			// Getters
			std::string GetVersion() const;
			int GetStatusCode() const;
			std::string GetReasonPhrase() const;
			std::string GetHeader(const std::string& key) const;
			std::unordered_map<std::string, std::string> GetHeaders() const;
			// Gets the body as a body subscriber
			template<typename T>
			BodySubscriber<T> GetBody() const;

			// Setters
			void SetVersion(const std::string& version);
			void SetStatusCode(int statusCode);
			void SetReasonPhrase(const std::string& reasonPhrase);
			void SetHeader(const std::string& key, const std::string& value);
			void SetHeaders(const std::unordered_map<std::string, std::string>& headers);
			// Sets the body as a body publisher
			template<typename T>
			void SetBody(const BodyPublisher<T>& body);
		};
	}
}