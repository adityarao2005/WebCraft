#include <memory>
#include "webcraft/util/async/async.h"
#include "webcraft/util/async/executors.hpp"

using namespace WebCraft::Util::Async;


std::unique_ptr<Executor> Executors::newSingleThreadExecutor() {
	return std::make_unique<SingleThreadExecutor>();

}
std::unique_ptr<Executor> Executors::newFixedThreadPoolExecutor(int nThreads) {
	return std::make_unique<FixedThreadPoolExecutor>(nThreads);

}
std::unique_ptr<Executor> Executors::newCachedThreadPoolExecutor() {
	throw std::runtime_error("Not implemented");
}
std::unique_ptr<Executor> Executors::newWorkStealingPoolExecutor(int nThreads) {
	throw std::runtime_error("Not implemented");
}
std::unique_ptr<Executor> Executors::newThreadPerTaskExecutor() {
	return std::make_unique<AsyncExecutor>();
}
std::unique_ptr<Executor> Executors::newCoroutineExecutor() {
	throw std::runtime_error("Not implemented");
}
std::unique_ptr<Executor> Executors::newFiberExecutor() {
	throw std::runtime_error("Not implemented");
}

std::unique_ptr<Executor> Executors::defaultExecutor = Executors::newFixedThreadPoolExecutor(std::thread::hardware_concurrency());

Executor* Executors::getDefaultExecutor() {
	return defaultExecutor.get();
}

void Executors::setDefaultExecutor(std::unique_ptr<Executor> executor) {
	defaultExecutor = std::move(executor);
}