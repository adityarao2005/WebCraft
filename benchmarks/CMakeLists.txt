cmake_minimum_required(VERSION 3.10)
project(WebCraftBenchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- WebCraft benchmark server ----
add_executable(webcraft_bench_server webcraft_server.cpp)

# Use the WebCraft library built by the parent project
target_link_libraries(webcraft_bench_server PRIVATE WebCraft)

# Ensure optimisations are on for meaningful benchmarks
if(CMAKE_BUILD_TYPE STREQUAL "" OR NOT CMAKE_BUILD_TYPE)
    message(STATUS "[benchmarks] No CMAKE_BUILD_TYPE set — defaulting to Release")
    set_target_properties(webcraft_bench_server PROPERTIES
        COMPILE_OPTIONS "-O2"
    )
endif()
