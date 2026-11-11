///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////


#define TEST_SUITE_NAME AsyncFSTestSuite

#include "test_suite.hpp"
#include <webcraft/async/async.hpp>
#include <webcraft/async/io/io.hpp>
#include <webcraft/async/runtime.hpp>
#include <filesystem>
#include <sstream>
#include <system_error>

using namespace webcraft::async;
using namespace webcraft::async::io::adaptors;
using namespace webcraft::async::io::fs;

TEST_CASE(TestFilePathCompatibility)
{
    std::filesystem::path p1("/path/to/file");

    auto f = make_file(p1);

    EXPECT_EQ(p1, f.get_path()) << "File path should match the original path";

    std::filesystem::path p2 = f;

    EXPECT_EQ(p2, p1) << "File path should match the original path";
}

const std::string test_data = "Hello, World!\r\nThis is some test data that is in the file\r\nWe need to use some kind of test procedure so we decided to go with this.\r\n";
const std::filesystem::path test_file_path = "test_file.txt";

void create_and_populate_test_file()
{
    std::ofstream ofs(test_file_path, std::ios::binary);
    ofs << test_data;
    ofs.close();
}

void cleanup_test_file()
{
    std::filesystem::remove(test_file_path);
}

std::string get_test_file_contents()
{
    std::ifstream ifs(test_file_path, std::ios::binary);
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

TEST_CASE(TestFileReadableStreamDoesNotTruncate)
{
    runtime_context context;

    create_and_populate_test_file();

    auto f = make_file(test_file_path);

    auto task_fn = [&]() -> task<void>
    {
        auto stream = co_await f.open_readable_stream();
    };

    sync_wait(task_fn());

    std::string content = get_test_file_contents();

    EXPECT_EQ(content, test_data) << "File contents should not be truncated";

    cleanup_test_file();
}

TEST_CASE(TestFileWritableStreamDoesTruncate)
{
    runtime_context context;

    create_and_populate_test_file();

    auto f = make_file(test_file_path);

    auto task_fn = [&]() -> task<void>
    {
        auto stream = co_await f.open_writable_stream();
    };

    sync_wait(task_fn());

    std::string content = get_test_file_contents();

    EXPECT_EQ(content, "") << "File contents should be truncated";

    cleanup_test_file();
}

TEST_CASE(TestFileAppendableStreamDoesNotTruncate)
{
    runtime_context context;

    create_and_populate_test_file();

    auto f = make_file(test_file_path);

    auto task_fn = [&]() -> task<void>
    {
        auto stream = co_await f.open_writable_stream(true);
    };

    sync_wait(task_fn());

    std::string content = get_test_file_contents();

    EXPECT_EQ(content, test_data) << "File contents should not be truncated";

    cleanup_test_file();
}

TEST_CASE(TestFileReadAll)
{
    runtime_context context;

    create_and_populate_test_file();

    auto f = make_file(test_file_path);

    auto task_fn = [&]() -> task<void>
    {
        auto stream = co_await f.open_readable_stream();

        std::cout << "Starting" << std::endl;
        std::vector<char> content;
        std::array<char, 1024> buffer;

        std::cout << "Reading bytes..." << std::endl;
        size_t count = 0;
        while (auto bytes_read = co_await stream.recv(buffer))
        {
            std::string_view view(buffer.data(), bytes_read);
            std::cout << "Read " << bytes_read << " bytes: " << view << std::endl;
            content.insert(content.end(), buffer.begin(), buffer.begin() + bytes_read);
            std::cout << "Reading bytes..." << std::endl;

            if (count++ > 5)
                throw std::runtime_error("Too many read calls");
        }
        std::cout << "No more bytes to read" << std::endl;

        std::string_view str(content.begin(), content.end());
        EXPECT_EQ(str, test_data) << "File contents should be the same";
    };

    sync_wait(task_fn());

    cleanup_test_file();
}

TEST_CASE(TestFileReadAllUsingAdaptors)
{
    runtime_context context;

    create_and_populate_test_file();

    auto f = make_file(test_file_path);

    auto task_fn = [&]() -> task<void>
    {
        auto stream = co_await f.open_readable_stream();

        std::vector<char> content = co_await (stream |
                                              collect<std::vector<char>, char>(collectors::to_vector<char>()));

        std::string_view str(content.begin(), content.end());
        EXPECT_EQ(str, test_data) << "File contents should be the same";
    };

    sync_wait(task_fn());

    cleanup_test_file();
}

TEST_CASE(TestFileWriteAll)
{
    runtime_context context;

    auto f = make_file(test_file_path);

    auto task_fn = [&]() -> task<void>
    {
        auto stream = co_await f.open_writable_stream();
        std::string data = test_data;
        co_await stream.send(data);
    };

    sync_wait(task_fn());

    std::string content = get_test_file_contents();

    EXPECT_EQ(content, test_data) << "File contents should be the same";

    cleanup_test_file();
}
TEST_CASE(TestFileEmptyEofAndMissingPath)
{
    runtime_context context;
    const auto path = std::filesystem::path("empty-regression-file.txt");
    std::ofstream(path, std::ios::binary).close();
    auto file = make_file(path);
    auto check = [&]() -> task<void>
    {
        auto stream = co_await file.open_readable_stream();
        std::array<char, 8> buffer{};
        EXPECT_EQ(co_await stream.recv(buffer), 0);
        EXPECT_FALSE(co_await stream.recv());
        co_await stream.close();
        co_await stream.close();
    };
    sync_wait(check());
    ASSERT_TRUE(std::filesystem::remove(path)) << "Empty-file fixture must be removed before the missing-file check";
    ASSERT_FALSE(std::filesystem::exists(path));
    EXPECT_THROW(sync_wait(file.open_readable_stream()), std::system_error);
}

TEST_CASE(TestMissingParentDirectoryOpenFails)
{
    runtime_context context;
    const auto path = std::filesystem::path("missing-parent-regression") / "file.txt";
    ASSERT_FALSE(std::filesystem::exists(path.parent_path())) << "Missing-parent fixture must not exist";
    auto file = make_file(path);
    EXPECT_THROW(sync_wait(file.open_readable_stream()), std::system_error);
    EXPECT_THROW(sync_wait(file.open_writable_stream()), std::system_error);
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_CASE(TestFileAppendAndMovedStreamRoundTrip)
{
    runtime_context context;
    create_and_populate_test_file();
    auto file = make_file(test_file_path);
    auto check = [&]() -> task<void>
    {
        auto original = co_await file.open_writable_stream(true);
        auto stream = std::move(original);
        std::string suffix = "appended";
        EXPECT_EQ(co_await stream.send(std::span<char>(suffix)), suffix.size());
        co_await stream.close();
        co_await stream.close();
        auto input = co_await file.open_readable_stream();
        std::vector<char> bytes;
        std::array<char, 7> buffer{};
        while (auto count = co_await input.recv(buffer))
            bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + count);
        EXPECT_EQ(std::string(bytes.begin(), bytes.end()), test_data + suffix);
        co_await input.close();
    };
    sync_wait(check());
    cleanup_test_file();
}

namespace
{
    using descriptor_base = webcraft::async::io::fs::detail::file_descriptor;
    struct delayed_descriptor : descriptor_base
    {
        async_event &gate;
        explicit delayed_descriptor(async_event &gate) : descriptor_base(std::ios::in), gate(gate) {}
        task<size_t> read(std::span<char>) override { co_return 0; }
        task<size_t> write(std::span<char>) override { co_return 0; }
        // The delayed operation borrows only the test gate. Descriptor ownership
        // is independently checked through weak_ptr while the stream closes.
        static task<void> wait_for_close(async_event &gate) { co_await gate; }
        task<void> close() override { return wait_for_close(gate); }
    };
}

TEST_CASE(TestStreamDestructionRetainsDescriptorUntilCloseCompletes)
{
    async_event gate;
    std::weak_ptr<delayed_descriptor> lifetime;
    {
        auto descriptor = std::make_shared<delayed_descriptor>(gate);
        lifetime = descriptor;
        file_rstream stream(std::move(descriptor));
    }
    EXPECT_FALSE(lifetime.expired()) << "Pending close must own the descriptor after stream destruction";
    gate.set();
    EXPECT_TRUE(lifetime.expired()) << "Completed close must release the descriptor";
}

TEST_CASE(TestStreamMoveAssignmentClosesPreviousDescriptor)
{
    async_event first_gate, second_gate;
    std::weak_ptr<delayed_descriptor> first_lifetime, second_lifetime;
    {
        auto first = std::make_shared<delayed_descriptor>(first_gate);
        auto second = std::make_shared<delayed_descriptor>(second_gate);
        first_lifetime = first;
        second_lifetime = second;
        file_rstream destination(std::move(first));
        file_rstream source(std::move(second));
        destination = std::move(source);
        EXPECT_FALSE(first_lifetime.expired()) << "Move assignment must finish closing the old descriptor";
        first_gate.set();
        EXPECT_TRUE(first_lifetime.expired());
        EXPECT_FALSE(second_lifetime.expired());
    }
    EXPECT_FALSE(second_lifetime.expired());
    second_gate.set();
    EXPECT_TRUE(second_lifetime.expired());
}
