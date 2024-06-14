#pragma once
#include "webcraft/webcraft.h"
#include "webcraft/util/async.h"
#include "webcraft/util/debug.h"

namespace WebCraft {
	namespace Networking {

		struct Server;
		struct Client;

		struct Server {
			struct sync {
				struct Request {
					std::string read(size_t size);
				};

				struct Response {
					void write(std::string data);
				};
			};

			struct async {
				struct Request {
					async(std::string) read(size_t size);
				};

				struct Response {
					async(void) write(std::string data);
				};
			};

			virtual void onconnect(sync::Request req, sync::Response resp) = 0;
			virtual async(void) onconnect(async::Request req, async::Response resp) = 0;


			Server() = default;
			~Server() = default;

			virtual void bind(int port) = 0;
			virtual void start() = 0;
			virtual void stop() = 0;
		};

		struct Client {
			struct sync {
				struct Request {
					void write(std::string data);
				};

				struct Response {
					std::string read(size_t size);
				};
			};

			struct async {
				struct Request {
					async(void) write(std::string data);
				};

				struct Response {
					async(std::string) read(size_t size);
				};
			};

			virtual void onconnect(sync::Request req, sync::Response resp) = 0;
			virtual async(void) onconnect(async::Request req, async::Response resp) = 0;
		};

	}
}