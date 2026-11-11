#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <random>
#include <syncstream>
#include <iostream>
#include <string_view>
#include <cstdint>
#include <chrono>
#include <webcraft/async/event_signal.hpp>
#include <optional>
#include <atomic>
#include <thread>
#include <memory>

using namespace std::chrono_literals;

#ifdef TEST_SUITE_NAME

#define TEST_CASE(name) TEST(TEST_SUITE_NAME, name)

#endif

inline uint64_t generate_random_uint64()
{
    static std::random_device rd;     // Seed source (hardware entropy)
    static std::mt19937_64 gen(rd()); // 64-bit Mersenne Twister engine
    static std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

    return dist(gen);
}

constexpr auto test_timer_timeout = 200ms;
constexpr auto test_cancel_timeout = 100ms;
constexpr auto test_adjustment_factor = 50ms;

namespace webcraft::test
{
    inline void trace(std::string_view phase, const char *file, int line)
    {
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        std::osyncstream(std::cerr) << "[webcraft ms=" << now << " thread=" << std::this_thread::get_id()
                                  << "] " << file << ':' << line << " " << phase << std::endl;
    }
}

#define TEST_TRACE(phase) ::webcraft::test::trace(phase, __FILE__, __LINE__)
