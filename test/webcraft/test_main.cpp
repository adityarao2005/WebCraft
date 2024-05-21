#include "test_web_math.h"
#include "test_executors.h"

int main() {
	std::cout << "Running tests..." << std::endl;
//	WebCraft::Util::RunUnitTest<web_math_test>();
//	WebCraft::Util::RunUnitTest<sockets_test>();
	WebCraft::Util::RunUnitTest<executors_test>();
	return 0;
}