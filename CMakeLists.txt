cmake_minimum_required(VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()


# https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# expected behaviour
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")

project(WebCraft)

# Configure for Windows and Linux
# include(cmake/PlatformChecks.cmake)  # Add a PlatformChecks.cmake file (explained later)

set(LIBRARY_HEADER_DIR include/)

# Add source files to library
add_executable(WebCraft "src/webcraft/web_math.cpp" "test/webcraft/test_main.cpp" "src/webcraft/util/debug.cpp" "test/webcraft/test_web_math.h" "src/webcraft/net/sockets.cpp" "test/webcraft/test_sockets.h" "src/webcraft/net/net.cpp" "test/webcraft/test_executors.h" "src/webcraft/util/async.cpp" "test/webcraft/test_net.h" "include/webcraft/http/http.h" "include/webcraft/util/async/task.h")

# Tell compiler where to find headers
target_include_directories(WebCraft PRIVATE ${LIBRARY_HEADER_DIR})

# Set compile definitions for the library
target_compile_definitions(WebCraft PUBLIC ${LIBRARY_DEFINITION})

# Target specific compiler flags for Windows/Linux (using PlatformChecks.cmake)
if(WIN32)
  # Add Windows specific flags here (e.g. /W3)
	target_link_libraries(WebCraft Ws2_32.lib)
elseif(UNIX)
  # Add Linux specific flags here (e.g. -Wall -Wextra)
endif()
