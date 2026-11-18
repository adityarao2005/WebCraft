cmake_minimum_required(VERSION 3.10)
project(WebCraftBenchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- WebCraft benchmark server ----
add_executable(webcraft_bench_server webcraft_server.cpp)

# Use the WebCraft library built by the parent project
target_link_libraries(webcraft_bench_server PRIVATE WebCraft)

# The benchmark and runtime must both use the selected Release configuration.
# Adding optimization flags to this executable alone would leave a Debug library.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(WARNING "Use -DCMAKE_BUILD_TYPE=Release for benchmark comparisons")
endif()
