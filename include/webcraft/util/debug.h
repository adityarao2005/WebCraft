#include "webcraft/webcraft.h"

namespace WebCraft {
	namespace Util {
		// Debugging utilities

		// Log levels
		enum LogLevel {
			INFO,
			WARNING,
			ERROR
		};

		// Log handler
		class LogHandler {
		public:
			// Log message
			virtual void log(std::string message, LogLevel level) = 0;
		};

		// Default log handler
		class DefaultLogHandler : public LogHandler {
		public:
			// Log message
			void log(std::string message, LogLevel level) override;
		};

		// Debugging class
		class Debug {
		public:
			// Logging
			static void log(std::string message, LogLevel level = INFO);

			// Exception handling
			static void throwException(std::string message);
			static void throwException(std::string message, std::exception e);
			static void throwException(std::exception e);

			// Assertion
			static void assert(bool condition, std::string message);
			
			// Log handler
			static void addLogHandler(LogHandler* handler);
		};
	}
}