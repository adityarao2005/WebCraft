#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <future>
#include <coroutine>
#include <iostream>
#include "webcraft/util/async.h"

namespace WebCraft {
	namespace Util {
		namespace Async {
			/*
			Your Task class template is a good start for a task-based asynchronous system. It provides a clear interface for running a task, checking its status, getting its result, and handling errors. However, there are a few things you might want to consider adding to make it more complete:
			1.	Task Dependencies: In many cases, you might have tasks that depend on the completion of other tasks. You could add a method to specify these dependencies, and then in your Run method, you would first check if all dependencies have completed before running the task.
			2.	Task Priorities: If you have many tasks and limited resources, you might want to prioritize some tasks over others. You could add a priority level to each task and use it to decide the order in which tasks are run.
			3.	Progress Reporting: For long-running tasks, it can be useful to report progress. You could add a method to set a progress callback function, which would be called periodically during the execution of the task.
			4.	Thread Safety: If your tasks are going to be accessed from multiple threads, you'll need to ensure that your class is thread-safe. This might involve adding locks to protect shared data.
			5.	Task Cancellation: You have a Cancel method, but it's not clear how it works. You might need to add some mechanism to stop the task if it's currently running, or to prevent it from starting if it hasn't started yet.
			6.	Error Handling: You have a GetError method that returns a std::unique_ptr<std::exception>. This is a good start, but you might want to consider how you're going to handle exceptions that are thrown during the execution of the task. One option is to catch them in the Run method and store them for later retrieval.
			7.	Task Completion Callbacks: It can be useful to specify a callback function that gets called when the task completes. This can be used to chain tasks together without having to constantly check their status.
			8.	Constructor: Your Create method returns a Task object, but it's not clear how the Task object is constructed since the constructor is not defined in your code. You'll need to add a constructor that takes a std::function (or the result of std::bind) and stores it for later execution.
			Remember, these are just suggestions. The exact features you need will depend on the specific requirements of your project.
			*/

			// enum talking about the status of the task
			enum TaskStatus {
				NotStarted,
				Running,
				Completed,
				Failed,
				Cancelled,
				Yielded
			};

			// interface for a task
			// alternative to futures
			template <typename T>
			class Task {
			private:
				// the task to run
				std::function<T()> m_task;
				TaskStatus m_status = NotStarted;
				std::unique_ptr<T> m_result;
				std::unique_ptr<std::exception> m_error;
				std::shared_ptr<std::unique_ptr<Executor>> m_executor = Executors::getDefaultExecutor();
				std::mutex m_mutex;

				Task(std::function<T()> task);
			public:
				// run the task
				virtual void Run() {
					try {
						m_status = Running;
						m_result = std::make_unique<T>(m_task());
						m_status = Completed;
					}
					catch (const std::exception& error) {
						m_error = std::make_unique<std::exception>(error);
						m_status = Failed;
					}
				}

				// operator to run the task
				inline void operator()() {
					Run();
				}

				// get the result of the task
				virtual TaskStatus GetStatus() {
					std::lock_guard<std::mutex> lock(m_mutex);
					return m_status;
				}

				// get the executor of the task
				std::shared_ptr<std::unique_ptr<Executor>> GetExecutor() {
					return m_executor;
				}

				void SetExecutor(std::unique_ptr<Executor> executor) {
					m_executor = executor;
				}

				// get the result of the task
				virtual T GetResult() {
					std::lock_guard<std::mutex> lock(m_mutex);
					switch (m_status) {
					case Failed:
						throw GetError().get();
					case Cancelled:
						throw std::runtime_error("Task was cancelled");
					case Running:
						while (m_status == Running);
						return GetResult();
					case NotStarted:
						throw std::runtime_error("Task not started");
					case Completed:
						return *m_result;
					}
				}

				// get the exception thrown by the task
				virtual std::unique_ptr<std::exception> GetError() {
					std::lock_guard<std::mutex> lock(m_mutex);
					return std::move(m_error);
				}

				// cancel the task
				virtual void Cancel() {
					std::lock_guard<std::mutex> lock(m_mutex);
					m_status = Cancelled;
				}

				// then apply a function to the result of the task
				template<typename Func, typename T_ = T, std::enable_if_t<!std::is_void<T_>::value>* = nullptr>
				auto ThenApply(Func continuation) -> Task<decltype(continuation(std::declval<T_>()))> {
					using V = decltype(continuation(std::declval<T_>()));

					return Task<V>::Create([this, continuation]() {
						Run();
						return continuation(GetResult());
						});
				}

				template<typename Func, typename T_ = T, std::enable_if_t<!std::is_void<T_>::value>* = nullptr>
				Task<void> ThenAccept(Func continuation) {
					return Task<void>::Create([this, continuation]() {
						Run();
						continuation(GetResult());
						});
				}

				// Create a delay
				static Task<void> Delay(std::chrono::milliseconds duration) {
					return Task<void>::Create([duration]() {
						std::this_thread::sleep_for(duration);
						});
				}

				// create a task with a function and its arguments
				template<typename Callable, typename... Args>
				static Task<T> Create(Callable&& callable, Args... args) {
					auto capturedCallable = std::forward<Callable>(callable);

					// Use a lambda to call the captured callable with arguments
					auto task = [capturedCallable, args...]() -> T {
						return capturedCallable(std::forward<Args>(args)...);
						};

					return Task<T>(task);
				}

			};
		}
	}
}