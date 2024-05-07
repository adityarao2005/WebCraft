#include <iostream>
#include "webcraft/web_math.h"

int main() {
	std::cout << "Hello, World!" << std::endl;
	std::cout << "2 + 3 = " << WebCraft::add(2, 3) << std::endl;
	std::cout << "2 - 3 = " << WebCraft::subtract(2, 3) << std::endl;
	return 0;
}