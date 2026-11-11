cmake_minimum_required(VERSION 3.10)
project(WebCraft VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 1. Dependencies ---
if (WIN32)
    set(WEBCRAFT_PLATFORM_LIBS ws2_32 Mswsock)
elseif (UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(uring REQUIRED IMPORTED_TARGET liburing)
    find_package(concurrentqueue CONFIG REQUIRED)
    set(WEBCRAFT_PLATFORM_LIBS PkgConfig::uring)
else()
    set(WEBCRAFT_PLATFORM_LIBS "")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(-fexperimental-library)
endif()

# --- 2. Build Target ---
file(GLOB_RECURSE WebCraft_SOURCES CONFIGURE_DEPENDS src/webcraft/*.cpp)
add_library(WebCraft STATIC ${WebCraft_SOURCES})

target_include_directories(WebCraft PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(WebCraft PUBLIC ${WEBCRAFT_PLATFORM_LIBS})

if (UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries(WebCraft PRIVATE concurrentqueue::concurrentqueue)
endif()

# Instrument both the library and its consumers (including template code in tests).
set(WEBCRAFT_SANITIZER "none" CACHE STRING "none, address, undefined, or address-undefined")
set_property(CACHE WEBCRAFT_SANITIZER PROPERTY STRINGS none address undefined address-undefined)
if(NOT WEBCRAFT_SANITIZER STREQUAL "none")
    if(WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        message(FATAL_ERROR "Sanitizer checks require GCC/Clang on Linux or macOS")
    endif()
    if(WEBCRAFT_SANITIZER STREQUAL "address-undefined")
        set(WEBCRAFT_SANITIZER_FLAGS "-fsanitize=address,undefined")
    elseif(WEBCRAFT_SANITIZER MATCHES "^(address|undefined)$")
        set(WEBCRAFT_SANITIZER_FLAGS "-fsanitize=${WEBCRAFT_SANITIZER}")
    else()
        message(FATAL_ERROR "Unknown WEBCRAFT_SANITIZER: ${WEBCRAFT_SANITIZER}")
    endif()
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "${WEBCRAFT_SANITIZER_FLAGS}")
    unset(WEBCRAFT_SANITIZER_SUPPORTED CACHE)
    check_cxx_source_compiles("int main() { return 0; }" WEBCRAFT_SANITIZER_SUPPORTED)
    unset(CMAKE_REQUIRED_FLAGS)
    if(NOT WEBCRAFT_SANITIZER_SUPPORTED)
        message(FATAL_ERROR "Compiler cannot compile/link ${WEBCRAFT_SANITIZER_FLAGS}")
    endif()
    target_compile_options(WebCraft PUBLIC ${WEBCRAFT_SANITIZER_FLAGS} -fno-omit-frame-pointer)
    target_link_libraries(WebCraft PUBLIC ${WEBCRAFT_SANITIZER_FLAGS})
endif()

# --- 3. Install Rules ---

# A. Install the Library and Headers
install(TARGETS WebCraft
    EXPORT WebCraftTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)

# B. Install the Targets File
# This tells CMake "Here are the binaries for WebCraft"
install(EXPORT WebCraftTargets
    FILE WebCraftTargets.cmake
    NAMESPACE WebCraft::
    DESTINATION share/WebCraft
)

# C. Generate and Install the Configuration File (THE FIX)
# This creates the "WebCraftConfig.cmake" that find_package() looks for.

include(CMakePackageConfigHelpers)

# 1. Create the Version file (so users can check if they have version 1.0.1)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/WebCraftConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# 2. Create the Config file content dynamically
# We need to ensure downstream users also find PkgConfig/liburing if on Linux
set(WEBCRAFT_CONFIG_CONTENT "
include(CMakeFindDependencyMacro)

# If we are on Linux (and not Android), we need to ensure liburing is found
if(UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(uring REQUIRED IMPORTED_TARGET liburing)
    find_dependency(concurrentqueue CONFIG)
endif()

# Include the targets file we exported earlier
include(\"\${CMAKE_CURRENT_LIST_DIR}/WebCraftTargets.cmake\")
")

# 3. Write the content to a file
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/WebCraftConfig.cmake" "${WEBCRAFT_CONFIG_CONTENT}")

# 4. Install the Config and Version files
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/WebCraftConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/WebCraftConfigVersion.cmake"
    DESTINATION share/WebCraft
)

# --- 4. Testing ---
option(WEBCRAFT_BUILD_TESTS "Build WebCraft tests" OFF)
if(WEBCRAFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- 5. Benchmarks ---
option(WEBCRAFT_BUILD_BENCHMARKS "Build WebCraft benchmark servers" OFF)
if(WEBCRAFT_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
