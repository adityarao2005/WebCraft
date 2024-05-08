#pragma once

// Enables static blocks to be used in C++ code
#define static_block(block) int __dummy__ = []() { \
block \
return 0; \
}();


