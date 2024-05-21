#pragma once
#include <iostream>
#include <thread>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "webcraft/util/debug.h"
#include "webcraft/util/unit_test.h"
#include "webcraft/util/async.h"

using namespace WebCraft::Util;
using namespace WebCraft::Util::Async;

class executors_test : public WebCraft::Util::UnitTest {
public:
	// To string method
	TO_STRING(executors_test);

	void beforeTest(const std::string& name) override {
		Debug::log("Before test: " + name);
	}

	void onTestFailed(const std::string& name, const std::exception& e) override {
		UnitTest::onTestFailed(name, e);
		Debug::log(std::string(e.what()), WebCraft::Util::LogLevel::ERROR);
	}

	void testExecutor(Executor* executor) {

		std::vector<std::future<int>> futures;

		for (int i = 0; i < 10; i++) {
			std::function<int()> func = [i]() {
				//std::stringstream ss;
				//ss << "Task " << i << " is running on thread " << std::this_thread::get_id() << std::endl;
				//Debug::log(ss.str());
				std::this_thread::sleep_for(std::chrono::seconds(10 - i));
				return i;
			};
			std::future<int> value = executor->execute(func);
			futures.push_back(std::move(value));
		}

		for (auto& future : futures) {
			try {
				int value = future.get();
				Debug::log("Numbers: " + std::to_string(value));
			}
			catch (const std::exception& e) {
				Debug::log("Line is: " + std::to_string(__LINE__), WebCraft::Util::LogLevel::ERROR);
				throw e;
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(10));
	}

	void test_single_thread_executor() {
		std::unique_ptr<Executor> executor = std::move(Executors::newSingleThreadExecutor());

		testExecutor(executor.get());

	}

	void test_fixed_thread_pool_executor() {
		std::unique_ptr<Executor> executor = std::move(Executors::newFixedThreadPoolExecutor(4));

		testExecutor(executor.get());
	}

	void Run() {
		RUN_TEST(executors_test, test_single_thread_executor);
		RUN_TEST(executors_test, test_fixed_thread_pool_executor);
	}
};