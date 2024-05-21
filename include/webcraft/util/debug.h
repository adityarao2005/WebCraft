#pragma once
#include "webcraft/webcraft.h"

namespace WebCraft {
	namespace Util {
		// Debugging utilities

		/// <summary>
		/// Represents the log level for logging.
		/// </summary>
		enum LogLevel {
			/// <summary>
			/// Logs info messages
			/// </summary>
			INFO = 0,
			/// <summary>
			/// Logs warning messages
			/// </summary>
			WARNING = 1,
			/// <summary>
			/// Logs error messages
			/// </summary>
			ERROR = 2,
			/// <summary>
			/// Logs fatal messages
			/// </summary>
			FATAL = 3,
		};

		/// <summary>
		/// Log handler interface for handling log messages.
		/// </summary>
		class LogHandler {
		public:
			/// <summary>
			/// Logs a message with the specified log level.
			/// </summary>
			/// <param name="message">Message to be logged</param>
			/// <param name="level">Log level</param>
			virtual void log(const std::string& message, LogLevel level) = 0;
		};

		/// <summary>
		/// Default log handler which just prints the log message to the console.
		/// </summary>
		class DefaultLogHandler : public LogHandler {
		public:
			/// <summary>
			/// Logs a message with the specified log level to the console.
			/// </summary>
			/// <param name="message">Message to be logged</param>
			/// <param name="level">Log level</param>
			void log(const std::string& message, LogLevel level) override;
		};

		/// <summary>
		/// Debugging utility class for logging, exception handling and assertion.
		/// </summary>
		class Debug {
		public:
			/// <summary>
			/// Logs a message with the specified log level.
			/// </summary>
			/// <param name="message">Message to be logged</param>
			/// <param name="level">Level to be logged at</param>
			static void log(const std::string& message, LogLevel level = LogLevel::INFO);

			/// <summary>
			/// Throws an exception with the specified message.
			/// </summary>
			/// <param name="message">message to be thrown</param>
			static void throwException(const std::string& message);
			/// <summary>
			/// Throws an exception with the specified message and exception.
			/// </summary>
			/// <param name="message">message to be thrown</param>
			/// <param name="e">exception to be thrown</param>
			static void throwException(const std::string& message, const std::exception& e);
			/// <summary>
			/// Throws an exception with the specified exception.
			/// </summary>
			/// <param name="e">exception to be thrown</param>
			static void throwException(const std::exception& e);

			/// <summary>
			/// Assertion
			/// </summary>
			/// <param name="condition">condition to assert</param>
			/// <param name="message">error message if false</param>
			static void assert(bool condition, const std::string& message = "Condition not met");
			
			/// <summary>
			/// Adds a log handler
			/// </summary>
			/// <param name="handler">handler to be added</param>
			static void addLogHandler(LogHandler* handler);
		};
	}
}