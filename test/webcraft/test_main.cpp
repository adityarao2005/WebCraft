#include "test_web_math.h"
#include "test_sockets.h"

int main() {
	WebCraft::Util::RunUnitTest<sockets_test>();
	//RUN_UNIT_TEST(sockets)
	return 0;
}