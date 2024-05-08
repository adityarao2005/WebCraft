#pragma once

#include "webcraft/web_math.h"
#include "webcraft/util/debug.h"
#include "webcraft/util/unit_test.h"

using namespace WebCraft::Util;

BEGIN_TEST_CLASS(web_math)

	BEGIN_TEST_METHOD(add)
		Debug::assert(WebCraft::add(4, 2) == 6);
	END_TEST_METHOD


	BEGIN_TEST_METHOD(subtract)
		Debug::assert(WebCraft::subtract(4, 2) == 2);
	END_TEST_METHOD


	BEGIN_TEST_METHOD(multiply)
		Debug::assert(WebCraft::multiply(4, 2) == 8);
	END_TEST_METHOD


	BEGIN_TEST_METHOD(divide)
		Debug::assert(WebCraft::divide(4, 2) == 3);
	END_TEST_METHOD

	BEGIN_UNIT_TEST
		ADD_TEST_METHOD(add)
		ADD_TEST_METHOD(subtract)
		ADD_TEST_METHOD(multiply)
		ADD_TEST_METHOD(divide)
	END_UNIT_TEST

END_TEST_CLASS