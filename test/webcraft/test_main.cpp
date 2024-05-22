#include "test_web_math.h"
#include "test_executors.h"
#include "test_net.h"

int main() {
	std::cout << "Running tests..." << std::endl;
//	WebCraft::Util::RunUnitTest<web_math_test>();
//	WebCraft::Util::RunUnitTest<sockets_test>();
//	WebCraft::Util::RunUnitTest<executors_test>();
	WebCraft::Util::RunUnitTest<net_test>();
	return 0;
}