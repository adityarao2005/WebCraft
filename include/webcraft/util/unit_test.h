#pragma once

namespace WebCraft {
	namespace Util {

		/// <summary>
		/// Represents a unit test.
		/// </summary>
		class UnitTest {
		public:
			/// <summary>
			/// Runs the unit test.
			/// </summary>
			virtual void Run() = 0;
		};

		// Begins a unit test class.
		#define BEGIN_TEST_CLASS(name) class name##__test : public WebCraft::Util::UnitTest { public:
		// Ends a unit test class.
		#define END_TEST_CLASS };

		// Begins a test method.
		#define BEGIN_TEST_METHOD(name) void name() {
		// Ends a test method.
		#define END_TEST_METHOD }

		// Begins the testing
		#define BEGIN_UNIT_TEST void Run() override { bool __testPassed = true;
		// Ends the testing
		#define END_UNIT_TEST }

		// Runs the unit test.
		#define RUN_UNIT_TEST(name) name##__test test; std::cout << "Unit Test Running: " << #name << std::endl; test.Run(); std::cout << "Unit Test Finished: " << #name << std::endl;

		// Adds a test method to the unit test.
		// Use Debug::assert & Debug::log for testing.
		#define ADD_TEST_METHOD(name) \
			try { name(); } \
			catch (const std::exception& e) { \
				__testPassed = false; \
			} \
				if (__testPassed) { \
					std::cout << "Test passed: " << #name << std::endl; \
				} else { \
					std::cout << "Test failed: " << #name << std::endl; \
				} \
				__testPassed = true;
			


	}
}