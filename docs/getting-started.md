# Getting started

## Prerequisites

Use a compiler and standard library with the C++23 features used by the project, CMake, Git, and the generator required by your preset. Linux and macOS presets use Ninja; Windows uses Visual Studio 2022. The macOS preset currently expects Ninja at `/opt/homebrew/bin/ninja`; override `CMAKE_MAKE_PROGRAM` if necessary.

Linux requires liburing and pkg-config. The vcpkg manifest supplies liburing and concurrentqueue on Linux, winsock2 on Windows, and GoogleTest. Clang builds currently enable `-fexperimental-library`; successful compilation also depends on the selected standard library.

## Build from source

From the repository root:

```sh
git submodule update --init --recursive
./vcpkg/bootstrap-vcpkg.sh
cmake --preset linux-build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

On macOS use `macos-build`. On Windows run `vcpkg\bootstrap-vcpkg.bat` and use `windows-build`. All presets use `build/`; use a separate build directory when switching compilers or generators.

## First coroutine

Save this as `main.cpp` in a separate application:

```cpp
#include <webcraft/async/runtime.hpp>
#include <iostream>

webcraft::async::task<int> answer()
{
    co_await webcraft::async::yield();
    co_return 42;
}

int main()
{
    webcraft::async::runtime_context runtime;
    std::cout << webcraft::async::sync_wait(answer()) << '\n';
}
```

Keep the runtime alive until its asynchronous work has completed. Tasks start eagerly; creating a task may execute code before you await it.

For an application building WebCraft from a checkout:

```cmake
cmake_minimum_required(VERSION 3.20)
project(example LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(path/to/WebCraft webcraft-build)
add_executable(example main.cpp)
target_link_libraries(example PRIVATE WebCraft)
```

Replace `path/to/WebCraft` with the checkout path. Configure the application with the vcpkg toolchain or otherwise provide the platform dependencies. Clang applications may also need the experimental library flag used by the library build.

## Installed library

Install a completed build to a prefix you control:

```sh
cmake --install build --config Debug --prefix /path/to/prefix
```

Consumers use `find_package(WebCraft CONFIG REQUIRED)` and link `WebCraft::WebCraft`; pass the install prefix through `CMAKE_PREFIX_PATH`. Linux consumers also need pkg-config and liburing available during configuration.

For tests and troubleshooting, see [Contributing](../CONTRIBUTING.md).
