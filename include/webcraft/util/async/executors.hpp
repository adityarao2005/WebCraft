#pragma once

#include "webcraft/util/async.h"
#include <thread>
#include <queue>
#include <mutex>
#include <memory>
#include "webcraft/util/debug.h"

namespace WebCraft {
	namespace Util {
		namespace Async {
			// Represents a single threaded executor
			class SingleThreadExecutor : public WebCraft::Util::Async::Executor {
			private:
				// Worker thread
				std::thread worker;
				// Task queue
				std::queue<std::function<void()>> tasks;
				// Mutex for synchronization
				std::mutex mutex;
				// Condition variable for synchronization
				std::condition_variable condition;
				// Tells whether to stop or not
				bool stop = false;

			public:
				SingleThreadExecutor() {
					// Start the worker thread
					worker = std::thread(&SingleThreadExecutor::EventLoop, this);
				}

				~SingleThreadExecutor() {
					// Shutdown the executor
					shutdown();
				}

				/// <summary>
				/// Shutsdown the executor and waits for all tasks to finish
				/// </summary>
				void shutdown() override {
					{
						// Lock the mutex
						std::unique_lock<std::mutex> lock(mutex);
						if (stop) return;
						stop = true;
					}
					// Notify the worker thread
					condition.notify_one();
					// Wait for the worker thread to finish
					if (worker.joinable())
						worker.join();
				}
			protected:

				/// <summary>
				/// Queues job to be executed
				/// </summary>
				/// <param name="task">task to be executed</param>
				void queue(std::function<void()> task) override {
					// Lock the mutex
					std::unique_lock<std::mutex> lock(mutex);
					// If stop is true, return
					if (stop)
						return;
					// Push the task to the queue
					tasks.push(task);
					// Notify the worker thread
					condition.notify_one();
				}


			private:
				/// <summary>
				/// Event loop for the worker thread
				/// <summary>
				void EventLoop() {

					// Run the loop
					while (true) {

						// Declare the task to be executed
						std::function<void()> task;
						{
							// Lock the mutex
							std::unique_lock<std::mutex> lock(mutex);
							// Wait for the task to be available
							condition.wait(lock, [this] { return stop || !tasks.empty(); });
							// If stop is true and no tasks are available, break
							if (stop && tasks.empty()) {
								break;
							}
							// Get the task
							task = std::move(tasks.front());
							// Pop the task
							tasks.pop();
						}
						// Execute the task
						task();

					}
				}
			};

			class FixedThreadPoolExecutor : public WebCraft::Util::Async::Executor {
			private:
				// Worker thread
				std::vector<std::thread> workers;
				// Task queue
				std::queue<std::function<void()>> tasks;
				// Mutex for synchronization
				std::mutex mutex;
				// Condition variable for synchronization
				std::condition_variable condition;
				// Tells whether to stop or not
				bool stop = false;

			public:
				FixedThreadPoolExecutor(size_t size) {
					// Start the worker thread
					for (size_t i = 0; i < size; i++) {
						workers.push_back(std::thread(&FixedThreadPoolExecutor::EventLoop, this));
					}
				}

				~FixedThreadPoolExecutor() {
					// Shutdown the executor
					shutdown();
				}

				/// <summary>
				/// Shutsdown the executor and waits for all tasks to finish
				/// </summary>
				void shutdown() override {
					{
						// Lock the mutex
						std::unique_lock<std::mutex> lock(mutex);
						if (stop) return;
						stop = true;
					}
					// Notify the worker thread
					condition.notify_one();
					// Wait for the worker thread to finish
					for (auto& worker : workers) {
						if (worker.joinable())
							worker.join();
					}
				}
			protected:

				/// <summary>
				/// Queues job to be executed
				/// </summary>
				/// <param name="task">task to be executed</param>
				void queue(std::function<void()> task) override {
					// Lock the mutex
					std::unique_lock<std::mutex> lock(mutex);
					// If stop is true, return
					if (stop)
						return;
					// Push the task to the queue
					tasks.push(task);
					// Notify the worker thread
					condition.notify_one();
				}


			private:
				/// <summary>
				/// Event loop for the worker thread
				/// <summary>
				void EventLoop() {
					// Run the loop
					while (true) {
						// Declare the task to be executed
						std::function<void()> task;
						{
							// Lock the mutex
							std::unique_lock<std::mutex> lock(mutex);
							// Wait for the task to be available
							condition.wait(lock, [this] { return stop || !tasks.empty(); });
							// If stop is true and no tasks are available, break
							if (stop && tasks.empty()) {
								break;
							}
							// Get the task
							task = std::move(tasks.front());
							// Pop the task
							tasks.pop();
						}
						// Execute the task
						task();
					}
				}
			};

		}
	}
}