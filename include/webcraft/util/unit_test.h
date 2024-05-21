#pragma once
#include <functional>
#include <future>
#include <string>
#include "debug.h"

namespace WebCraft {
	namespace Util {

		/// <summary>
		/// Represents a unit test.
		/// </summary>
		class UnitTest {
		public:
			virtual std::string to_string() = 0;
			/// <summary>
			/// Runs the unit test.
			/// </summary>
			virtual void Run() {};

			UnitTest() {}
			~UnitTest() {}

			/// <summary>
			/// Called when a test fails.
			/// </summary>
			virtual void onTestFailed(const std::string& name, const std::exception& e) {
				Debug::log("Test failed: " + name, LogLevel::ERROR);
			}

			/// <summary>
			/// Called when a test passes.
			/// </summary>
			virtual void onTestPassed(const std::string& name) {
				Debug::log("Test passed: " + name);
			}

			virtual void beforeTest(const std::string& name) {}

			/// <summary>
			/// Runs a test asynchronously.
			///	</summary>
			std::future<void> RunTestAsync(std::function<void()> method, const std::string& name) {
				return std::async(std::launch::async, &UnitTest::RunTest, this, method, name);
			}

			/// <summary>
			/// Runs a test.
			/// </summary>
			void RunTest(std::function<void()> method, const std::string& name) {
				bool __testPassed = true;
				beforeTest(name);
				try { method(); }
				catch (const std::exception& e) {
					__testPassed = false;
					onTestFailed(name, e);
				}
				if (__testPassed) {
					onTestPassed(name);
				}
			}

		};

		template<class T>
		static void RunUnitTest() {
			T test;
			std::cout << "Unit Test Running: " << test.to_string() << std::endl;
			test.Run();
			std::cout << "Unit Test Finished: " << test.to_string() << std::endl;
		}

		template<class T>
		static void RunUnitTestAsync() {
			std::future<void> future = std::async(std::launch::async, []() {
				RunUnitTest<T>();
			});
		}

#define TO_STRING(x) std::string to_string() { return #x; }

		// Runs a unit test method.
#define RUN_TEST(class_name, method_name) RunTest(std::bind(& class_name :: method_name , this), #method_name);

		// Runs a unit test method asynchronously.
#define RUN_TEST_ASYNC(class_name, method_name) std::future<void> method_name##_future = RunTestAsync(std::bind(& class_name :: method_name , this), #method_name);

	}
}