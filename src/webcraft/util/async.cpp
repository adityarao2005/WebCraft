#include <memory>
#include "webcraft/util/async.h"
#include "webcraft/util/async/executors.hpp"

using namespace WebCraft::Util::Async;

// Represents a single threaded executor
std::unique_ptr<Executor> Executors::newSingleThreadExecutor() {
	// Return the executor
	return std::make_unique<SingleThreadExecutor>();
}


// Represents a fixed thread pool executor
std::unique_ptr<Executor> Executors::newFixedThreadPoolExecutor(int nThreads) {
	// Return the executor
	return std::make_unique<FixedThreadPoolExecutor>(nThreads);
}

// Represents a cached thread pool executor
std::unique_ptr<Executor> Executors::newCachedThreadPoolExecutor() {
	throw std::runtime_error("Not implemented");
}

// Represents a work stealing pool executor
std::unique_ptr<Executor> Executors::newWorkStealingPoolExecutor(int nThreads) {
	throw std::runtime_error("Not implemented");
}

// Represents an async executor
std::unique_ptr<Executor> Executors::newAsyncExecutor() {
	throw std::runtime_error("Not implemented");
}

// Represents a coroutine executor
std::unique_ptr<Executor> Executors::newCoroutineExecutor() {
	throw std::runtime_error("Not implemented");
}

// Represents a fiber executor
std::unique_ptr<Executor> Executors::newFiberExecutor() {
	throw std::runtime_error("Not implemented");
}