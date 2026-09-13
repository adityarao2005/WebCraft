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
