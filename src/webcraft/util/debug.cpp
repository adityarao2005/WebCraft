#include "webcraft/util/debug.h"
#include "webcraft/util/statics.h"

using namespace WebCraft::Util;

// Default log handler
void DefaultLogHandler::log(const std::string& message, LogLevel level) {
	std::cout << "[" << level << "] " << message << std::endl;
}

// Log handlers
std::vector<LogHandler*> handlers;

// Dummy value to just add the default log handler
static_block({
	handlers.push_back(new DefaultLogHandler());
})

// Debug logging
// Debug::log method
void Debug::log(const std::string& message, LogLevel level) {
	for (LogHandler* handler : handlers) {
		handler->log(message, level);
	}
}

// Debug::addLogHandler method
void Debug::addLogHandler(LogHandler* handler) {
	handlers.push_back(handler);
}

// Debug exception handling
// Debug::throwException method
void Debug::throwException(const std::string& message) {
	throw std::runtime_error(message);
}

// Debug::throwException method
void Debug::throwException(const std::string& message, const std::exception& e) {
	throwException(message + ": " + e.what());
}

// Debug::throwException method
void Debug::throwException(const std::exception& e) {
	throw e;
}

// Debug assertion handling
// Debug::assert method
void Debug::assert(bool condition, const std::string& message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}
