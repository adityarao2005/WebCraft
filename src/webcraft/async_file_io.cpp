///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <webcraft/async/io/io.hpp>
#include <webcraft/async/runtime.hpp>
#include <webcraft/async/task_completion_source.hpp>
#include <webcraft/async/runtime/windows.event.hpp>
#include <webcraft/async/runtime/macos.event.hpp>
#include <webcraft/async/runtime/linux.event.hpp>
#include <cstdio>
#include <cerrno>
#include <webcraft/async/thread_pool.hpp>
#include <webcraft/async/async_event.hpp>
#include <system_error>

using namespace webcraft::async;
using namespace webcraft::async::io::fs;
using namespace webcraft::async::io::fs::detail;

#if defined(WEBCRAFT_MOCK_FS_TESTS)

class sync_file_descriptor : public file_descriptor
{
private:
    std::FILE *file = nullptr;

public:
    sync_file_descriptor(std::filesystem::path p, std::ios_base::openmode mode) : file_descriptor(mode)
    {
        if (mode & std::ios::in)
        {
            file = std::fopen(p.c_str(), "r");
        }
        else if (mode & std::ios::out)
        {
            if (mode & std::ios::app)
            {
                file = std::fopen(p.c_str(), "a");
            }
            else
            {
                file = std::fopen(p.c_str(), "w");
            }
        }

        if (!file)
        {
            const int error = errno;
            throw std::system_error(error, std::generic_category(), "Failed to open file: " + p.string());
        }
    }

    ~sync_file_descriptor()
    {
        if (file)
        {
            fire_and_forget(close());
            file = nullptr;
        }
    }

    // virtual because we want to allow platform specific implementation
    task<size_t> read(std::span<char> buffer)
    {
        co_return std::fread(buffer.data(), sizeof(char), buffer.size(), file);
    }

    task<size_t> write(std::span<char> buffer)
    {
        co_return std::fwrite(buffer.data(), sizeof(char), buffer.size(), file);
    }

    task<void> close()
    {
        if (file)
        {
            std::fclose(file);
            file = nullptr;
        }
        co_return;
    }
};

task<std::shared_ptr<file_descriptor>> webcraft::async::io::fs::detail::make_file_descriptor(std::filesystem::path p, std::ios_base::openmode mode)
{
    co_return std::make_shared<sync_file_descriptor>(p, mode);
}

#elif defined(__linux__)

int ios_to_posix(std::ios_base::openmode mode)
{
    int flags = 0;

    if ((mode & std::ios::in) && (mode & std::ios::out))
    {
        flags |= O_RDWR;
    }
    else if (mode & std::ios::in)
    {
        flags |= O_RDONLY;
    }
    else if (mode & std::ios::out)
    {
        flags |= O_WRONLY;
    }

    if (mode & std::ios::trunc)
    {
        flags |= O_TRUNC;
    }

    if (mode & std::ios::app)
    {
        flags |= O_APPEND;
    }

    if (mode & std::ios::out)
    {
        flags |= O_CREAT; // often needed with out/app/trunc
    }

    return flags;
}

class io_uring_file_descriptor : public webcraft::async::io::fs::detail::file_descriptor
{
private:
    int fd;
    bool closed{false};

public:
    io_uring_file_descriptor(int fd, std::ios_base::openmode mode) : file_descriptor(mode), fd(fd)
    {
    }

    ~io_uring_file_descriptor()
    {
        if (!closed)
        {
            fire_and_forget(close());
        }
    }

    // virtual because we want to allow platform specific implementation
    task<size_t> read(std::span<char> buffer) override
    {
        if ((mode & std::ios::in) != std::ios::in)
        {
            throw std::logic_error("File not open for reading");
        }

        int fd = this->fd;
        auto event = webcraft::async::detail::as_awaitable(webcraft::async::detail::linux::create_io_uring_event([fd, buffer](struct io_uring_sqe *sqe)
                                                                                                                 { io_uring_prep_read(sqe, fd, buffer.data(), buffer.size(), -1); }));

        co_await event;

        co_return event.get_result();
    }

    task<size_t> write(std::span<char> buffer) override
    {
        if ((mode & std::ios::out) != std::ios::out)
        {
            throw std::logic_error("File not open for writing");
        }

        int fd = this->fd;
        auto event = webcraft::async::detail::as_awaitable(webcraft::async::detail::linux::create_io_uring_event([fd, buffer](struct io_uring_sqe *sqe)
                                                                                                                 { io_uring_prep_write(sqe, fd, buffer.data(), buffer.size(), 0); }));

        co_await event;

        co_return event.get_result();
    }

    task<void> close() override
    {
        if (closed)
            co_return;

        int fd = this->fd;

        co_await webcraft::async::detail::as_awaitable(webcraft::async::detail::linux::create_io_uring_event([fd](struct io_uring_sqe *sqe)
                                                                                                             { io_uring_prep_close(sqe, fd); }));

        closed = true;
    }
};

task<std::shared_ptr<file_descriptor>> webcraft::async::io::fs::detail::make_file_descriptor(std::filesystem::path p, std::ios_base::openmode mode)
{
    int flags = ios_to_posix(mode);

    auto event = webcraft::async::detail::as_awaitable(webcraft::async::detail::linux::create_io_uring_event([flags, p](struct io_uring_sqe *sqe)
                                                                                                             { io_uring_prep_open(sqe, p.c_str(), flags, 0644); }, {}));

    co_await event;

    int fd = event.get_result();
    if (fd < 0)
    {
        std::error_code ec(-fd, std::system_category());
        throw std::system_error(ec, "Failed to open file: " + p.string());
    }

    co_return std::make_shared<io_uring_file_descriptor>(fd, mode);
}

#elif defined(_WIN32)

#include <webcraft/async/runtime/windows.event.hpp>

class iocp_file_descriptor : public file_descriptor
{
protected:
    HANDLE fd;
    HANDLE iocp;
    LONGLONG fileOffset = 0;

public:
    iocp_file_descriptor(std::filesystem::path p, std::ios_base::openmode mode) : file_descriptor(mode)
    {
        // Implementation for Windows
        DWORD desiredAccess = 0;
        DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
        DWORD creationMode = OPEN_EXISTING; // Default for read-only

        if ((mode & std::ios::in) == std::ios::in)
        {
            // Read only
            desiredAccess = GENERIC_READ;
            creationMode = OPEN_EXISTING;
        }
        else if ((mode & std::ios::out) == std::ios::out)
        {
            // Write only
            if ((mode & std::ios::trunc) == std::ios::trunc)
            {
                desiredAccess = GENERIC_WRITE;
                creationMode = CREATE_ALWAYS;
            }
            else if ((mode & std::ios::app) == std::ios::app)
            {
                desiredAccess = FILE_APPEND_DATA;
                creationMode = OPEN_ALWAYS;
            }
            else
            {
                desiredAccess = GENERIC_WRITE;
                creationMode = CREATE_ALWAYS;
            }
        }

        fd = ::CreateFileW(p.c_str(), desiredAccess, shareMode, nullptr, creationMode, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);

        if (fd == INVALID_HANDLE_VALUE)
        {
            throw webcraft::async::detail::windows::overlapped_runtime_event_error("Failed to create file");
        }

        // Associate this file handle with the global IOCP
        iocp = ::CreateIoCompletionPort(fd, (HANDLE)webcraft::async::detail::get_native_handle(), 0, 0);

        if (iocp == nullptr)
        {
            CloseHandle(fd);
            throw webcraft::async::detail::windows::overlapped_runtime_event_error("Failed to associate file with IO completion port");
        }
    }

    ~iocp_file_descriptor()
    {
        if (fd != INVALID_HANDLE_VALUE)
        {
            fire_and_forget(close());
        }
    }

    task<size_t> read(std::span<char> buffer)
    {
        if ((mode & std::ios::in) == std::ios::in)
        {
            LONGLONG offset = fileOffset;
            HANDLE fd = this->fd;
            auto event = webcraft::async::detail::as_awaitable(
                webcraft::async::detail::windows::create_async_io_overlapped_event(
                    fd,
                    [fd, buffer, offset](LPDWORD bytesTransferred, LPOVERLAPPED ptr)
                    {
                        ptr->Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
                        ptr->OffsetHigh = static_cast<DWORD>((offset >> 32) & 0xFFFFFFFF);
                        return ::ReadFile(fd, buffer.data(), (ULONG)buffer.size(), bytesTransferred, ptr);
                    }));
            co_await event;

            fileOffset += event.get_result();

            co_return event.get_result();
        }
        throw std::logic_error("The file is not opened in read mode");
    }

    task<size_t> write(std::span<char> buffer)
    {
        if ((mode & std::ios::out) == std::ios::out)
        {
            LONGLONG offset = fileOffset;
            HANDLE fd = this->fd;
            auto event = webcraft::async::detail::as_awaitable(
                webcraft::async::detail::windows::create_async_io_overlapped_event(
                    fd,
                    [fd, buffer, offset](LPDWORD bytesTransferred, LPOVERLAPPED ptr)
                    {
                        ptr->Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
                        ptr->OffsetHigh = static_cast<DWORD>((offset >> 32) & 0xFFFFFFFF);
                        return ::WriteFile(fd, buffer.data(), (ULONG)buffer.size(), bytesTransferred, ptr);
                    }));
            co_await event;

            if (event.get_result() < 0)
            {
                throw webcraft::async::detail::windows::overlapped_runtime_event_error("Writing the file failed with error code");
            }

            fileOffset += event.get_result();

            co_return event.get_result();
        }
        throw std::logic_error("The file is not opened in write mode");
    }

    task<void> close()
    {
        if (fd != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(fd);
            fd = INVALID_HANDLE_VALUE;
        }
        // Don't close iocp as it's the global IOCP handle
        co_return;
    }
};

task<std::shared_ptr<file_descriptor>> webcraft::async::io::fs::detail::make_file_descriptor(std::filesystem::path p, std::ios_base::openmode mode)
{
    co_return std::make_shared<iocp_file_descriptor>(p, mode);
}

#elif defined(__APPLE__)

static webcraft::async::thread_pool pool(std::thread::hardware_concurrency(), std::thread::hardware_concurrency() * 2);

class thread_pool_file_descriptor : public file_descriptor
{
private:
    std::FILE *file = nullptr;

public:
    thread_pool_file_descriptor(std::filesystem::path p, std::ios_base::openmode mode) : file_descriptor(mode)
    {
        if (mode & std::ios::in)
        {
            file = std::fopen(p.c_str(), "r");
        }
        else if (mode & std::ios::out)
        {
            if (mode & std::ios::app)
            {
                file = std::fopen(p.c_str(), "a");
            }
            else
            {
                file = std::fopen(p.c_str(), "w");
            }
        }

        if (!file)
        {
            const int error = errno;
            throw std::system_error(error, std::generic_category(), "Failed to open file: " + p.string());
        }
    }

    ~thread_pool_file_descriptor()
    {
        if (file)
        {
            fire_and_forget(close());
            file = nullptr;
        }
    }

    // virtual because we want to allow platform specific implementation
    task<size_t> read(std::span<char> buffer)
    {
        task_completion_source<size_t> source;

        pool.submit([&]
                    {
            auto size = std::fread(buffer.data(), sizeof(char), buffer.size(), file);
            source.set_value(size); });

        auto si = co_await source.task();
        co_await yield();
        co_return si;
    }

    task<size_t> write(std::span<char> buffer)
    {

        task_completion_source<size_t> source;

        pool.submit([&]
                    {
            auto size = std::fwrite(buffer.data(), sizeof(char), buffer.size(), file);
            source.set_value(size); });

        auto si = co_await source.task();
        co_await yield();
        co_return si;
    }

    task<void> close()
    {
        if (file)
        {
            std::fclose(file);
            file = nullptr;
        }
        co_return;
    }
};

task<std::shared_ptr<file_descriptor>> webcraft::async::io::fs::detail::make_file_descriptor(std::filesystem::path p, std::ios_base::openmode mode)
{
    co_return std::make_shared<thread_pool_file_descriptor>(p, mode);
}

#else
#error "Async file IO not implemented for this platform"
#endif
