#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#ifdef __APPLE__

#include <atomic>
#include <webcraft/async/runtime.hpp>
#include <sys/event.h>
#include <unistd.h>

namespace webcraft::async::detail::macos
{

    class kqueue_runtime_error : public std::exception
    {
    public:
        explicit kqueue_runtime_error(std::string message) : msg_(message) {}
        virtual const char *what() const noexcept override
        {
            return msg_.c_str();
        }

    private:
        std::string msg_;
    };

    struct kqueue_runtime_event : public webcraft::async::detail::runtime_event
    {
    private:
        std::atomic<bool> cancelled{false};
        std::atomic<bool> started{false};
        struct kevent event{};
        int queue;

    public:
        kqueue_runtime_event(std::stop_token token) : runtime_event(token)
        {
            queue = (int)webcraft::async::detail::get_native_handle();
        }

        ~kqueue_runtime_event()
        {
            if (!cancelled.load(std::memory_order_acquire))
                try_native_cancel();
        }

        void try_native_cancel() override
        {
            bool expected = false;
            if (!cancelled.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;

            // Only try to delete the event if it was actually registered
            if (started.load(std::memory_order_acquire))
            {
                // remove yield event listener
                EV_SET(&event, event.ident, event.filter, EV_DELETE, 0, 0, nullptr);
                int result = kevent(queue, &event, 1, nullptr, 0, nullptr);
            }
        }

        void try_start() override
        {
            if (cancelled.load(std::memory_order_acquire))
                return;

            // listen to the yield event
            void *data = (webcraft::async::detail::runtime_callback *)this;

            prep_event(&event, data);
            started.store(true, std::memory_order_release);
        }

        virtual void prep_event(struct kevent *event, void *data) = 0;
    };

    using kqueue_operation = std::function<void(struct kevent *, void *)>;

    inline auto create_kqueue_event(kqueue_operation op, std::stop_token token = webcraft::async::get_stop_token())
    {
        struct kqueue_runtime_event_impl : public kqueue_runtime_event
        {
            kqueue_runtime_event_impl(kqueue_operation op, std::stop_token token) : op(op), kqueue_runtime_event(token) {}

            void prep_event(struct kevent *event, void *data) override
            {
                op(event, data);
            }

        private:
            kqueue_operation op;
        };

        return std::make_unique<kqueue_runtime_event_impl>(op, token);
    }
}

#endif