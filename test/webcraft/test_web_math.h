#pragma once

#include "webcraft/web_math.h"
#include "webcraft/util/debug.h"
#include "webcraft/util/unit_test.h"


class web_math_test : public WebCraft::Util::UnitTest {
public:
	void test_add() {
		WebCraft::Util::Debug::assert(WebCraft::add(4, 2) == 6);
	}

	void test_subtract() {
		WebCraft::Util::Debug::assert(WebCraft::subtract(4, 2) == 2);
	}


	void test_multiply() {
		WebCraft::Util::Debug::assert(WebCraft::multiply(4, 2) == 8);
	}

	void test_divide() {
		WebCraft::Util::Debug::assert(WebCraft::divide(4, 2) == 2);
	}

	void Run() override {
		// Run test using std::bind
		RunTest(std::bind(&web_math_test::test_add, this), "test_add");
		RunTest(std::bind(&web_math_test::test_subtract, this), "test_subtract");
		RunTest(std::bind(&web_math_test::test_multiply, this), "test_multiply");
		RunTest(std::bind(&web_math_test::test_divide, this), "test_divide");

		// Run test using macros
		RUN_TEST(web_math_test, test_add);
		RUN_TEST(web_math_test, test_subtract);
		RUN_TEST(web_math_test, test_multiply);
		RUN_TEST(web_math_test, test_divide);

		// Run test using std::bind
		RunTestAsync(std::bind(&web_math_test::test_add, this), "test_add");
		RunTestAsync(std::bind(&web_math_test::test_subtract, this), "test_subtract");
		RunTestAsync(std::bind(&web_math_test::test_multiply, this), "test_multiply");
		RunTestAsync(std::bind(&web_math_test::test_divide, this), "test_divide");

		// Run test using macros
		RUN_TEST_ASYNC(web_math_test, test_add);
		RUN_TEST_ASYNC(web_math_test, test_subtract);
		RUN_TEST_ASYNC(web_math_test, test_multiply);
		RUN_TEST_ASYNC(web_math_test, test_divide);
	}

	TO_STRING(web_math_test);

};