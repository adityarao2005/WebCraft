///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////


#define TEST_SUITE_NAME AsyncIOAdaptorsTestSuite

#include "test_suite.hpp"
#include <webcraft/async/async.hpp>
#include <webcraft/async/io/core.hpp>
#include <webcraft/async/io/adaptors.hpp>
#include <string>
#include <vector>
#include <deque>
#include <span>
#include <functional>
#include <algorithm>
#include <numeric>
#include <sstream>

using namespace webcraft::async;
using namespace webcraft::async::io;
using namespace webcraft::async::io::adaptors;
using namespace std::chrono_literals;

template <typename T>
class mock_readable_stream
{
public:
    explicit mock_readable_stream(std::vector<T> values) : values(std::move(values)) {}

    task<std::optional<T>> recv()
    {
        if (values.empty())
        {
            co_return std::nullopt;
        }
        T value = std::move(values.front());
        values.erase(values.begin());
        co_return std::make_optional(std::move(value));
    }

private:
    std::vector<T> values;
};

template <typename T>
class mock_writable_stream
{
private:
    std::queue<T> received_values;

public:
    task<bool> send(T value)
    {
        received_values.push(value);
        co_return true;
    }

    bool received(T value)
    {
        if (received_values.empty())
        {
            throw std::logic_error("You're trying to check if a value is received from an empty queue. Try sending something first.");
        }

        auto received = received_values.front();
        received_values.pop();
        return received == value;
    }
};

static_assert(async_readable_stream<mock_readable_stream<int>, int>, "mock_readable_stream should be an async readable stream");
static_assert(async_writable_stream<mock_writable_stream<int>, int>, "mock_writable_stream should be an async writable stream");
static_assert(async_readable_stream<mock_readable_stream<std::string>, std::string>, "mock_readable_stream should be an async readable stream");
static_assert(async_writable_stream<mock_writable_stream<std::string>, std::string>, "mock_writable_stream should be an async writable stream");

TEST_CASE(TestTransformStreamAdaptor)
{
    std::vector<int> values({1, 2, 3, 4, 5});
    mock_readable_stream<int> stream(values);

    auto transform_fn = [](async_generator<int> gen) -> async_generator<int>
    {
        for_each_async(value, gen,
                       {
                           co_yield value * 2 - 1; // Transform by multiplying each value by 2
                           co_yield value * 2;     // Transform by multiplying each value by 2
                       });
    };

    auto task_fn = [&]() -> task<void>
    {
        auto transformed_stream = stream | transform<int>(std::move(transform_fn));
        std::vector<int> results;

        while (auto value = co_await transformed_stream.recv())
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 10) << "Should read five values from the transformed stream";
        size_t counter = 0;
        for (size_t val : values)
        {
            auto check = val * 2 - 1;
            EXPECT_EQ(check, results[counter]) << "Expected: " << check << ", Actual: " << results[counter];
            counter++;
            check = val * 2;
            EXPECT_EQ(check, results[counter]) << "Expected: " << check << ", Actual: " << results[counter];
            counter++;
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestTransformStreamAdaptorReturningString)
{
    mock_readable_stream<int> stream({1, 2, 3, 4, 5});

    auto transform_fn = [](async_generator<int> gen) -> async_generator<std::string>
    {
        for_each_async(value, gen,
                       {
                           co_yield std::to_string(value * 2); // Transform by multiplying each value by 2 and converting to string
                       });
    };

    auto task_fn = [&]() -> task<void>
    {
        auto transformed_stream = stream | transform<int>(std::move(transform_fn));
        std::vector<std::string> results;

        while (auto value = co_await transformed_stream.recv())
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 5) << "Should read five values from the transformed stream";
        for (size_t i = 0; i < results.size(); ++i)
        {
            EXPECT_EQ(results[i], std::to_string((i + 1) * 2)) << "Value at index " << i << " should be " << (i + 1) * 2;
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestMapStreamAdaptor)
{
    mock_readable_stream<int> stream({1, 2, 3, 4, 5});

    auto map_fn = [](int value) -> std::string
    {
        return "Value: " + std::to_string(value * 2); // Map by multiplying each value by 2 and converting to string
    };

    auto task_fn = [&]() -> task<void>
    {
        auto mapped_stream = stream | map<int>(std::move(map_fn));
        std::vector<std::string> results;

        while (auto value = co_await mapped_stream.recv())
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 5) << "Should read five values from the mapped stream";
        for (size_t i = 0; i < results.size(); ++i)
        {
            EXPECT_EQ(results[i], "Value: " + std::to_string((i + 1) * 2)) << "Value at index " << i << " should be 'Value: " << (i + 1) * 2 << "'";
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestPipeStreamAdaptor)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});
    mock_writable_stream<int> wstream;

    auto task_fn = [&]() -> task<void>
    {
        auto stream = rstream | pipe<int>(wstream);
        std::vector<int> received_from_stream;

        while (auto value = co_await stream.recv())
        {
            auto real_val = *value;
            received_from_stream.push_back(real_val);
        }

        // Now check that all values were received by the writable stream
        EXPECT_EQ(received_from_stream.size(), 5) << "Should have received 5 values from stream";
        for (int expected_val : received_from_stream)
        {
            EXPECT_TRUE(wstream.received(expected_val)) << "Should have received: " << expected_val;
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestCollectorAdaptor)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto collector_func = [](async_generator<int> gen) -> task<std::string>
    {
        std::stringstream strstream;

        strstream << "[";

        auto itr = co_await gen.begin();
        strstream << std::to_string(*itr);
        co_await ++itr;

        while (itr != gen.end())
        {
            strstream << "," << std::to_string(*itr);
            co_await ++itr;
        }

        strstream << "]";
        co_return strstream.str();
    };

    auto task_fn = [&]() -> task<void>
    {
        auto str = co_await (rstream | collect<std::string, int>(std::move(collector_func)));
        EXPECT_EQ(str, "[1,2,3,4,5]");
    };

    sync_wait(task_fn());
}

TEST_CASE(TestForwardToAdaptor)
{
    std::vector<int> values({1, 2, 3, 4, 5});

    mock_readable_stream<int> rstream(values);
    mock_writable_stream<int> wstream;

    auto task_fn = [&]() -> task<void>
    {
        co_await (rstream | forward_to<int>(wstream));
    };

    sync_wait(task_fn());

    EXPECT_EQ(values.size(), 5) << "Should have sent 5 values to the writable stream";
    for (int val : values)
    {
        EXPECT_TRUE(wstream.received(val)) << "Should have received: " << val;
    }
}

TEST_CASE(TestFilterAdaptor)
{
    std::vector<int> values({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    mock_readable_stream<int> rstream(values);

    auto filter_fn = [](int value) -> bool
    {
        return value % 2 == 0; // Filter to keep only even numbers
    };

    auto task_fn = [&]() -> task<void>
    {
        auto filtered_stream = rstream | filter<int>(std::move(filter_fn));
        std::vector<int> results;

        while (auto value = co_await filtered_stream.recv())
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 5) << "Should read five values from the filtered stream";
        for (size_t i = 0; i < results.size(); ++i)
        {
            EXPECT_EQ(results[i], (i + 1) * 2) << "Value at index " << i << " should be " << (i + 1) * 2;
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestTakeAdaptor)
{
    std::vector<int> values({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    mock_readable_stream<int> rstream(values);

    auto task_fn = [&]() -> task<void>
    {
        auto taken_stream = rstream | limit<int>(5);
        std::vector<int> results;

        while (auto value = co_await taken_stream.recv())
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 5) << "Should read five values from the taken stream";
        for (size_t i = 0; i < results.size(); ++i)
        {
            EXPECT_EQ(results[i], i + 1) << "Value at index " << i << " should be " << (i + 1);
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestDropAdaptor)
{
    std::vector<int> values({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    mock_readable_stream<int> rstream(values);

    auto task_fn = [&]() -> task<void>
    {
        auto dropped_stream = rstream | skip<int>(5);
        std::vector<int> results;

        while (auto value = co_await dropped_stream.recv())
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 5) << "Should read five values from the dropped stream";
        for (size_t i = 0; i < results.size(); ++i)
        {
            EXPECT_EQ(results[i], i + 6) << "Value at index " << i << " should be " << (i + 6);
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestTakeWhileAdaptor)
{
    std::vector<int> values({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    mock_readable_stream<int> rstream(values);

    auto take_while_fn = [](int value) -> bool
    {
        return value < 6; // Take while the value is less than 6
    };

    auto task_fn = [&]() -> task<void>
    {
        auto taken_stream = rstream | take_while<int>(std::move(take_while_fn));
        std::vector<int> results;

        while (auto value = co_await taken_stream.recv())
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 5) << "Should read five values from the taken stream";
        for (size_t i = 0; i < results.size(); ++i)
        {
            EXPECT_EQ(results[i], i + 1) << "Value at index " << i << " should be " << (i + 1);
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestDropWhileAdaptor)
{
    std::vector<int> values({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    mock_readable_stream<int> rstream(values);

    auto drop_while_fn = [](int value) -> bool
    {
        return value < 6; // Drop while the value is less than 6
    };

    auto task_fn = [&]() -> task<void>
    {
        auto dropped_stream = rstream | drop_while<int>(std::move(drop_while_fn));
        std::vector<int> results;

        while (auto value = co_await dropped_stream.recv())
        {
            results.push_back(std::move(*value));
        }

        EXPECT_EQ(results.size(), 5) << "Should read five values from the dropped stream";
        for (size_t i = 0; i < results.size(); ++i)
        {
            EXPECT_EQ(results[i], i + 6) << "Value at index " << i << " should be " << (i + 6);
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestComplexAdaptorExample)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    mock_writable_stream<std::string> wstream, piped_stream;

    auto task_fn = [&]() -> task<void>
    {
        auto complex_stream = rstream | filter<int>([](int value)
                                                    { return value % 2 == 0; }) // Filter even numbers
                              | take_while<int>([](int value)
                                                { return value < 8; }) // Take while less than 8
                              | transform<int>([](async_generator<int> gen) -> async_generator<int>
                                               { for_each_async(value, gen,
                                                                {
                                                                    co_yield value * 2 - 1;
                                                                    co_yield value * 2;
                                                                }); }) |
                              drop_while<int>([](int value)
                                              { return value < 5; }) // Drop while less than 5
                              | map<int>([](int value)
                                         { return "Transformed: " + std::to_string(value); })       // Map to string
                              | pipe<std::string>(piped_stream) | forward_to<std::string>(wstream); // Forward to writable stream

        co_await complex_stream;

        // Now check that all values were received by the writable stream
        std::vector<std::string> expected_values = {"Transformed: 7", "Transformed: 8"};
        for (const std::string &expected_val : expected_values)
        {
            EXPECT_TRUE(wstream.received(expected_val)) << "Should have received: " << expected_val;
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestAdaptorsWithChannel)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto [reader, writer] = make_mpsc_channel<std::string>();

    auto task_fn = [&]() -> task<void>
    {
        auto complex_stream = rstream | filter<int>([](int value)
                                                    { return value % 2 == 0; }) // Filter even numbers
                              | take_while<int>([](int value)
                                                { return value < 8; }) // Take while less than 8
                              | transform<int>([](async_generator<int> gen) -> async_generator<int>
                                               { for_each_async(value, gen,
                                                                {
                                                                    co_yield value * 2 - 1;
                                                                    co_yield value * 2;
                                                                }); }) |
                              drop_while<int>([](int value)
                                              { return value < 5; }) // Drop while less than 5
                              | map<int>([](int value)
                                         { 
                                            std::string mapped = "Transformed: " + std::to_string(value);
                                            std::cout << "Mapping value: " << value << ", To: \"" << mapped << "\"" << std::endl;
                                            return mapped; })                    // Map to string
                              | forward_to<std::string>(writer); // Forward to writable stream

        co_await complex_stream;

        // Now check that all values were received by the writable stream
        std::vector<std::string> expected_values = {"Transformed: 7", "Transformed: 8"};
        size_t count = 0;

        for (const std::string &expected_val : expected_values)
        {
            if (auto value = co_await reader.recv())
            {
                EXPECT_EQ(*value, expected_val) << "Should have received: " << expected_val;
                count++;
            }
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestReduceCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto reduce_fn = [](int a, int b) -> int
    {
        return a + b; // Reduce by summing all values
    };

    auto task_fn = [&]() -> task<void>
    {
        auto reduced_value = co_await (rstream | collect<int, int>(collectors::reduce<int>(std::move(reduce_fn))));
        EXPECT_EQ(reduced_value, 15) << "Should have reduced to the sum of all values";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestJoiningCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto task_fn = [&]() -> task<void>
    {
        auto joined_value = co_await (rstream | map<int>([](int value)
                                                         { return std::to_string(value); }) |
                                      collect<std::string, std::string>(collectors::joining<std::string>(",", "[", "]")));
        EXPECT_EQ(joined_value, "[1,2,3,4,5]") << "Should have joined all values";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestToVectorCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto task_fn = [&]() -> task<void>
    {
        auto vector_value = co_await (rstream | collect<std::vector<int>, int>(collectors::to_vector<int>()));
        EXPECT_EQ(vector_value.size(), 5) << "Should have collected all values into a vector";
        for (size_t i = 0; i < vector_value.size(); ++i)
        {
            EXPECT_EQ(vector_value[i], i + 1) << "Value at index " << i << " should be " << (i + 1);
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestGroupByCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

    auto group_by_fn = [](const int &value) -> int
    {
        return value % 3; // Group by remainder when divided by 3
    };

    auto task_fn = [&]() -> task<void>
    {
        auto grouped_map = co_await (rstream | collect<std::unordered_map<int, std::vector<int>>, int>(
                                                   collectors::group_by<int, int>(std::move(group_by_fn))));

        EXPECT_EQ(grouped_map.size(), 3) << "Should have three groups based on the grouping function";
        for (const auto &[key, values] : grouped_map)
        {
            EXPECT_LE(values.size(), 4) << "Each group should have four values";
            for (int value : values)
            {
                EXPECT_EQ(value % 3, key) << "Value " << value << " should belong to group " << key;
            }
        }
    };

    sync_wait(task_fn());
}

TEST_CASE(TestMinCollector)
{
    mock_readable_stream<int> rstream({5, 3, 8, 1, 4});

    auto task_fn = [&]() -> task<void>
    {
        auto min_value = co_await (rstream | min<int>());
        EXPECT_EQ(min_value, 1) << "Should have found the minimum value in the stream";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestMaxCollector)
{
    mock_readable_stream<int> rstream({5, 3, 8, 1, 4});

    auto task_fn = [&]() -> task<void>
    {
        auto max_value = co_await (rstream | max<int>());
        EXPECT_EQ(max_value, 8) << "Should have found the maximum value in the stream";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestSumCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto task_fn = [&]() -> task<void>
    {
        auto sum_value = co_await (rstream | sum<int>());
        EXPECT_EQ(sum_value, 15) << "Should have summed all values in the stream";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestFindFirstCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto find_first_fn = [](const int &value) -> bool
    {
        return value > 3; // Find first value greater than 3
    };

    auto task_fn = [&]() -> task<void>
    {
        auto found_value = co_await (rstream | find_first<int>(std::move(find_first_fn)));
        EXPECT_EQ(*found_value, 4) << "Should have found the first value greater than 3";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestEmptyFindFirstCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3});

    auto find_first_fn = [](const int &value) -> bool
    {
        return value > 5; // No value greater than 5
    };

    auto task_fn = [&]() -> task<void>
    {
        auto found_value = co_await (rstream | find_first<int>(std::move(find_first_fn)));
        EXPECT_FALSE(found_value.has_value()) << "Should not have found any value greater than 5";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestFindLastCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto find_last_fn = [](const int &value) -> bool
    {
        return value < 5; // Find last value less than 5
    };

    auto task_fn = [&]() -> task<void>
    {
        auto found_value = co_await (rstream | find_last<int>(std::move(find_last_fn)));
        EXPECT_EQ(*found_value, 4) << "Should have found the last value less than 5";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestEmptyFindLastCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3});

    auto find_last_fn = [](const int &value) -> bool
    {
        return value > 5; // No value greater than 5
    };

    auto task_fn = [&]() -> task<void>
    {
        auto found_value = co_await (rstream | find_last<int>(std::move(find_last_fn)));
        EXPECT_FALSE(found_value.has_value()) << "Should not have found any value greater than 5";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestAnyMatchesCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto any_matches_fn = [](const int &value) -> bool
    {
        return value > 3; // Check if any value is greater than 3
    };

    auto task_fn = [&]() -> task<void>
    {
        auto check = co_await (rstream | any_matches<int>(std::move(any_matches_fn)));
        EXPECT_TRUE(check) << "Should have found at least one value greater than 3";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestNegAnyMatchesCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3});

    auto any_matches_fn = [](const int &value) -> bool
    {
        return value > 3; // Check if any value is greater than 3
    };

    auto task_fn = [&]() -> task<void>
    {
        auto check = co_await (rstream | any_matches<int>(std::move(any_matches_fn)));
        EXPECT_FALSE(check) << "Should not have found any value greater than 3";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestAllMatchesCollector)
{
    mock_readable_stream<int> rstream({2, 4, 6, 8, 10});

    auto all_matches_fn = [](const int &value) -> bool
    {
        return value % 2 == 0; // Check if all values are even
    };

    auto task_fn = [&]() -> task<void>
    {
        auto check = co_await (rstream | all_matches<int>(std::move(all_matches_fn)));
        EXPECT_TRUE(check) << "Should have found all values to be even";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestNegAllMatchesCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto all_matches_fn = [](const int &value) -> bool
    {
        return value % 2 == 0; // Check if all values are even
    };

    auto task_fn = [&]() -> task<void>
    {
        auto check = co_await (rstream | all_matches<int>(std::move(all_matches_fn)));
        EXPECT_FALSE(check) << "Should not have found all values to be even";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestNoneMatchesCollector)
{
    mock_readable_stream<int> rstream({1, 3, 5, 7, 9});

    auto none_matches_fn = [](const int &value) -> bool
    {
        return value % 2 == 0; // Check if no values are even
    };

    auto task_fn = [&]() -> task<void>
    {
        auto check = co_await (rstream | none_matches<int>(std::move(none_matches_fn)));
        EXPECT_TRUE(check) << "Should have found no even values in the stream";
    };

    sync_wait(task_fn());
}

TEST_CASE(TestNegNoneMatchesCollector)
{
    mock_readable_stream<int> rstream({1, 2, 3, 4, 5});

    auto none_matches_fn = [](const int &value) -> bool
    {
        return value % 2 == 0; // Check if no values are even
    };

    auto task_fn = [&]() -> task<void>
    {
        auto check = co_await (rstream | none_matches<int>(std::move(none_matches_fn)));
        EXPECT_FALSE(check) << "Should not have found no even values in the stream";
    };

    sync_wait(task_fn());
}
TEST_CASE(TestInferredPipelineOwnsTemporaryAdaptors)
{
    auto make_pipeline = []
    {
        mock_readable_stream<int> input({1, 2, 3, 4, 5, 6});
        return input | map([offset = std::make_unique<int>(10)](auto value) { return value + *offset; })
                     | filter([](auto value) { return value % 2 == 0; })
                     | skip(1) | limit(2)
                     | map([](int value) { return std::to_string(value); })
                     | filter([](const std::string &value) { return value != "16"; });
    };
    auto pipeline = make_pipeline();
    auto result = sync_wait(pipeline | collect(collectors::to_vector()));
    EXPECT_EQ(result, (std::vector<std::string>{"14"}));
}

TEST_CASE(TestReusableInferredAdaptorAcrossElementTypes)
{
    auto twice = map([](auto value) { return value + value; });
    mock_readable_stream<int> integers({1, 2});
    mock_readable_stream<std::string> strings({"a", "bc"});
    EXPECT_EQ(sync_wait(integers | twice | collect(collectors::to_vector())), (std::vector<int>{2, 4}));
    EXPECT_EQ(sync_wait(strings | twice | collect(collectors::to_vector())), (std::vector<std::string>{"aa", "bcbc"}));
}

TEST_CASE(TestInferredTransformPredicatesAndSinks)
{
    mock_readable_stream<int> input({1, 2, 3, 4, 5});
    mock_writable_stream<std::string> copied, output;
    auto pipeline = input | transform([](async_generator<int> gen) -> async_generator<int>
                                      { for_each_async(value, gen, { co_yield value * 2; }); })
                          | drop_while([](auto value) { return value < 4; })
                          | take_while([](auto value) { return value < 9; })
                          | map([](auto value) { return std::to_string(value); })
                          | pipe(copied);
    sync_wait(pipeline | forward_to(output));
    for (const auto &value : {"4", "6", "8"})
    {
        EXPECT_TRUE(copied.received(value));
        EXPECT_TRUE(output.received(value));
    }
}

TEST_CASE(TestInferredCollectors)
{
    auto input = [] { return mock_readable_stream<int>({3, 1, 2}); };
    EXPECT_EQ(sync_wait(input() | min()), 1);
    EXPECT_EQ(sync_wait(input() | max()), 3);
    EXPECT_EQ(sync_wait(input() | sum()), 6);
    EXPECT_EQ(sync_wait(input() | find_first([](auto value) { return value > 1; })), 3);
    EXPECT_EQ(sync_wait(input() | find_last([](auto value) { return value > 1; })), 2);
    EXPECT_TRUE(sync_wait(input() | any_matches([](auto value) { return value == 2; })));
    EXPECT_TRUE(sync_wait(input() | all_matches([](auto value) { return value > 0; })));
    EXPECT_TRUE(sync_wait(input() | none_matches([](auto value) { return value == 0; })));
    EXPECT_EQ(sync_wait(input() | collect(collectors::reduce([](auto a, auto b) { return a + b; }))), 6);
    EXPECT_EQ(sync_wait(input() | map([](auto value) { return std::to_string(value); })
                               | collect(collectors::joining(",", "[", "]"))), "[3,1,2]");
    auto grouped = sync_wait(input() | collect(collectors::group_by([](auto value) { return value % 2; })));
    EXPECT_EQ(grouped[0], (std::vector<int>{2}));
    EXPECT_EQ(grouped[1], (std::vector<int>{3, 1}));
    mock_readable_stream<int> empty({});
    EXPECT_TRUE(sync_wait(empty | collect(collectors::to_vector())).empty());
}

TEST_CASE(TestInferredPipelineAcrossSuspension)
{
    auto [reader, writer] = make_mpsc_channel<int>();
    auto consume = [&]() -> task<std::vector<int>>
    {
        co_return co_await (reader | map([offset = 4](auto value) { return value + offset; })
                                  | collect(collectors::to_vector()));
    };
    auto pending = consume();
    sync_wait(writer.send(1));
    sync_wait(writer.send(2));
    sync_wait(writer.close());
    EXPECT_EQ(sync_wait(std::move(pending)), (std::vector<int>{5, 6}));
}
