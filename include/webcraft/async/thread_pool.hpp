#pragma once

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Aditya Rao
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

/**
 * @file async/thread_pool.hpp
 * @brief Cooperative thread pool for executing queued work items.
 *
 * This header exposes the thread-pool implementation used by runtime and
 * async helper code to dispatch CPU-bound or deferred work.
 */

#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stop_token>
#include <unordered_map>
#include <queue>
#include <set>
#include <future>
#include <functional>
#include <type_traits>

using namespace std::chrono_literals;

namespace webcraft::async
{

    /**
     * @brief Exception reported when submitting work after shutdown.
     */
    class thread_pool_shutdown_error : public std::runtime_error
    {
    public:
        thread_pool_shutdown_error() : std::runtime_error("Thread pool is shutting down") {}
    };

    class thread_pool;

    /**
     * @brief Worker object that owns one pool thread.
     */
    class thread_pool_worker
    {
    private:
        std::jthread thr;
        thread_pool *pool;

    public:
        thread_pool_worker(thread_pool *pool);

        ~thread_pool_worker()
        {
            request_stop();
        }

        void request_stop()
        {
            thr.request_stop();
        }

        void run_loop(std::stop_token token);

        std::thread::id get_id()
        {
            return thr.get_id();
        }
    };

    /**
     * @brief Dynamic thread pool for executing callable work items.
     */
    class thread_pool
    {
    private:
        const size_t min_threads, max_threads;
        const std::chrono::milliseconds idle_timeout;
        std::unordered_map<std::thread::id, std::unique_ptr<thread_pool_worker>> workers;
        std::vector<std::unique_ptr<thread_pool_worker>> workers_to_remove;
        std::queue<std::function<void()>> tasks;
        mutable std::mutex mutex;
        std::condition_variable cv;
        std::atomic<int> available_workers{0};
        std::atomic<bool> shutdown{false};
        friend class thread_pool_worker;

    public:
        thread_pool(size_t min_threads = 0, size_t max_threads = std::thread::hardware_concurrency(), std::chrono::milliseconds idle_timeout = 10'000ms) : min_threads(min_threads), max_threads(max_threads), idle_timeout(idle_timeout)
        {
            for (size_t i = 0; i < min_threads; i++)
            {
                auto worker = std::make_unique<thread_pool_worker>(this);
                workers[worker->get_id()] = std::move(worker);
            }
        }

        ~thread_pool()
        {
            try_shutdown();
        }

        void try_shutdown()
        {
            bool expected = false;
            if (shutdown.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            {

                // Request stop for all workers
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    for (auto &it : workers)
                    {
                        it.second->request_stop();
                    }
                }

                cv.notify_all();

                // Wait for all workers to finish and clean up
                workers.clear();
            }
        }

        /**
         * @brief Enqueues callable work and returns a future for its result.
         */
        template <typename F, typename... Args>
        auto submit(F &&f, Args &&...args) -> std::future<typename std::invoke_result_t<F, Args...>>
        {
            using return_type = typename std::invoke_result_t<F, Args...>;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));

            std::future<return_type> result = task->get_future();

            if (shutdown.load())
            {
                // Return a future with an exception if shutting down
                std::promise<return_type> promise;
                try
                {
                    throw thread_pool_shutdown_error();
                }
                catch (...)
                {
                    promise.set_exception(std::current_exception());
                }
                return promise.get_future();
            }

            {
                std::unique_lock<std::mutex> lock(mutex);

                // Clean up workers marked for removal
                cleanup_workers();

                // Check if we need to create a new worker
                if (available_workers.load() == 0 && workers.size() < max_threads)
                {
                    auto worker = std::make_unique<thread_pool_worker>(this);
                    auto worker_id = worker->get_id();
                    workers[worker_id] = std::move(worker);
                }

                tasks.push([task]()
                           { (*task)(); });
            }
            cv.notify_one();

            return result;
        }

    public:
        const size_t get_min_threads() const
        {
            return min_threads;
        }

        const size_t get_max_threads() const
        {
            return max_threads;
        }

        const std::chrono::milliseconds get_idle_timeout() const
        {
            return idle_timeout;
        }

        const size_t get_workers_size() const
        {
            std::unique_lock<std::mutex> lock(mutex);
            return workers.size();
        }

        const size_t get_available_workers() const
        {
            return available_workers.load();
        }

    private:
        void cleanup_workers()
        {
            workers_to_remove.clear();
        }
    };

    inline thread_pool_worker::thread_pool_worker(thread_pool *pool) : pool(pool)
    {
        pool->available_workers.fetch_add(1);
        thr = std::jthread([this](std::stop_token token)
                           { run_loop(token); });
    }

    inline void thread_pool_worker::run_loop(std::stop_token token)
    {
        while (!token.stop_requested() && !pool->shutdown.load())
        {
            std::function<void()> task;
            bool has_task = false;

            {
                std::unique_lock<std::mutex> lock(pool->mutex);

                auto status = pool->cv.wait_for(lock, pool->get_idle_timeout(), [this, &token]()
                                                { return !pool->tasks.empty() || token.stop_requested() || pool->shutdown.load(); });

                if (!status && pool->tasks.empty() && !pool->shutdown.load())
                {
                    // Timeout occurred and no tasks available
                    if (pool->workers.size() > pool->get_min_threads())
                    {
                        // Mark this worker for removal instead of removing immediately
                        auto worker = std::move(pool->workers.at(get_id()));
                        pool->workers.erase(get_id());
                        pool->workers_to_remove.push_back(std::move(worker));
                        pool->available_workers.fetch_sub(1);
                        return;
                    }
                }

                if (token.stop_requested() || pool->shutdown.load())
                {
                    pool->available_workers.fetch_sub(1);
                    return;
                }

                if (!pool->tasks.empty())
                {
                    task = std::move(pool->tasks.front());
                    pool->tasks.pop();
                    has_task = true;
                }
            }
            pool->available_workers.fetch_sub(1);

            // Execute task with exception handling
            if (has_task)
            {
                try
                {
                    task();
                }
                catch (...)
                {
                    // Log exception or handle appropriately
                    // For now, we'll just continue to prevent thread termination
                }
            }
            pool->available_workers.fetch_add(1);
        }
    }
}