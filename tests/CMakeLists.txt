# Tests/CMakeLists.txt

find_package(GTest CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(webcraft_tests ${TEST_SOURCES})
target_include_directories(webcraft_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(webcraft_tests PRIVATE WebCraft GTest::gtest)

include(GoogleTest)
gtest_discover_tests(webcraft_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES TIMEOUT 10
)
