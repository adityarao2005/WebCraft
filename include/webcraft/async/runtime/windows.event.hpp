#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <sstream>
#include <webcraft/async/runtime.hpp>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <system_error>

namespace webcraft::async::detail::windows
{
    struct overlapped_event : public OVERLAPPED
    {
        webcraft::async::detail::runtime_callback *event;

        overlapped_event(webcraft::async::detail::runtime_callback *ev) : event(ev)
        {
            ZeroMemory(this, sizeof(OVERLAPPED));
        }
    };

    using overlapped_operation = std::function<BOOL(LPDWORD, LPOVERLAPPED)>;
    using overlapped_cancel = std::function<void(LPOVERLAPPED)>;

    inline std::system_error overlapped_runtime_event_error(std::string message)
    {
        std::error_code ec(GetLastError(), std::system_category());
        return std::system_error(ec, message);
    }

    inline std::system_error overlapped_winsock2_runtime_error(std::string message)
    {
        std::error_code ec(WSAGetLastError(), std::system_category());
        return std::system_error(ec, message);
    }

    struct overlapped_runtime_event : public webcraft::async::detail::runtime_event
    {
    protected:
        overlapped_event overlapped;

    public:
        overlapped_runtime_event(std::stop_token token) : runtime_event(token), overlapped(this)
        {
        }

        virtual ~overlapped_runtime_event() = default;

        void try_start() override
        {
            DWORD numberOfBytesTransfered = 0;
            BOOL result = perform_overlapped_operation(&numberOfBytesTransfered, &overlapped);

            if (result)
            {
                // Operation completed synchronously
                try_execute(numberOfBytesTransfered);
            }
            else if (!result && GetLastError() != ERROR_IO_PENDING)
            {
                // Operation failed
                throw overlapped_runtime_event_error("Failed to post overlapped event");
            }
        }

        void try_native_cancel() override
        {
            try_native_cancel(&overlapped);
        }

        virtual void try_native_cancel(LPOVERLAPPED overlapped) = 0;
        virtual BOOL perform_overlapped_operation(LPDWORD numberOfBytesTransfered, LPOVERLAPPED overlapped) = 0;
    };

    struct overlapped_async_io_runtime_event : public overlapped_runtime_event
    {
    private:
        HANDLE file;

    public:
        overlapped_async_io_runtime_event(HANDLE file, std::stop_token token) : overlapped_runtime_event(token), file(file)
        {
        }

        ~overlapped_async_io_runtime_event()
        {
        }

        void try_start() override
        {
            DWORD numberOfBytesTransfered = 0;
            BOOL result = perform_overlapped_operation(&numberOfBytesTransfered, &overlapped);

            // Associated handles keep completion notifications enabled. Even
            // synchronous success queues an IOCP packet, which owns completion
            // and keeps the OVERLAPPED/event alive until the packet is consumed.
            if (!result && GetLastError() != ERROR_IO_PENDING)
            {
                throw overlapped_runtime_event_error("Failed to post overlapped I/O");
            }
        }

        void try_native_cancel(LPOVERLAPPED overlapped) override
        {
            BOOL result = CancelIoEx(file, overlapped);
            if (!result && GetLastError() != ERROR_NOT_FOUND)
            {
                throw overlapped_runtime_event_error("Failed to cancel IO operation");
            }
        }
    };

    inline auto create_overlapped_event(overlapped_operation op, overlapped_cancel cancel = [](LPOVERLAPPED) {}, std::stop_token token = get_stop_token())
    {
        struct overlapped_runtime_event_impl : public overlapped_runtime_event
        {

            overlapped_runtime_event_impl(overlapped_operation op, overlapped_cancel cancel, std::stop_token token)
                : overlapped_runtime_event(token), op(std::move(op)), cancel(std::move(cancel))
            {
            }

            BOOL perform_overlapped_operation(LPDWORD numberOfBytesTransfered, LPOVERLAPPED overlapped) override
            {
                return op(numberOfBytesTransfered, overlapped);
            }

            void try_native_cancel(LPOVERLAPPED overlapped) override
            {
                cancel(overlapped);
            }

        private:
            overlapped_operation op;
            overlapped_cancel cancel;
        };

        return std::make_unique<overlapped_runtime_event_impl>(std::move(op), std::move(cancel), token);
    }

    inline auto create_async_io_overlapped_event(HANDLE file, overlapped_operation op, std::stop_token token = get_stop_token())
    {
        struct overlapped_async_io_runtime_event_impl : public overlapped_async_io_runtime_event
        {
            overlapped_async_io_runtime_event_impl(HANDLE file, overlapped_operation op, std::stop_token token)
                : overlapped_async_io_runtime_event(file, token), op(std::move(op))
            {
            }

            BOOL perform_overlapped_operation(LPDWORD numberOfBytesTransfered, LPOVERLAPPED overlapped) override
            {
                return op(numberOfBytesTransfered, overlapped);
            }

        private:
            overlapped_operation op;
        };

        return std::make_unique<overlapped_async_io_runtime_event_impl>(file, std::move(op), token);
    }

    inline auto create_async_socket_overlapped_event(SOCKET file, overlapped_operation op, std::stop_token token = get_stop_token())
    {
        struct overlapped_async_socket_runtime_event_impl : public overlapped_async_io_runtime_event
        {
            overlapped_async_socket_runtime_event_impl(SOCKET file, overlapped_operation op, std::stop_token token)
                : overlapped_async_io_runtime_event((HANDLE)file, token), op(std::move(op))
            {
            }

            BOOL perform_overlapped_operation(LPDWORD numberOfBytesTransfered, LPOVERLAPPED overlapped) override
            {
                int result = op(numberOfBytesTransfered, overlapped);
                if (result == SOCKET_ERROR)
                {
                    int error = WSAGetLastError();
                    if (error != WSA_IO_PENDING)
                    {
                        throw overlapped_winsock2_runtime_error("Failed to perform overlapped operation");
                    }
                    return FALSE; // Operation is pending
                }
                WSASetLastError(WSA_IO_PENDING);
                return FALSE; // Operation completed synchronously but itll still send an overlapped event
            }

        private:
            overlapped_operation op;
        };

        return std::make_unique<overlapped_async_socket_runtime_event_impl>(file, std::move(op), token);
    }
}

#endif