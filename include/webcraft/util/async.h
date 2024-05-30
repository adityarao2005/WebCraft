#pragma once

#include <functional>
#include <memory>
#include <future>
#include "webcraft/util/debug.h"

#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <type_traits>
#include <condition_variable>


namespace WebCraft {
	namespace Util {
		namespace Async {

			/// <summary>
			/// Represents an executor for asynchronous tasks.
			/// </summary>
			class Executor {
			protected:
				/// <summary>
				/// Queues a task to be executed.
				/// </summary>
				virtual void queue(std::function<void()> task) {
					std::cout << "HaHa you missed" << std::endl;
				}
			public:
				/// <summary>
				/// Executes a task asynchronously.
				/// </summary>
				template <typename F, typename... Args>
				constexpr decltype(auto) execute(F&& f, Args&&... args)
				{
					using type = std::packaged_task<std::invoke_result_t<F, Args...>()>;
					auto task = std::make_shared<type>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

					auto fut = task->get_future();

					queue([task] { (*task)(); });

					return fut;
				}


				/// <summary>
				/// Shuts executor down and waits for all tasks to finish.
				/// </summary>
				virtual void shutdown() {}

				/// <summary>
				/// Returns true if the executor is running.
				/// </summary>
				virtual bool isRunning() { return false; }
			};

			class Executors {
			private:
				static std::shared_ptr<std::unique_ptr<Executor>> defaultExecutor;
			public:
				static std::unique_ptr<Executor> newSingleThreadExecutor();
				static std::unique_ptr<Executor> newFixedThreadPoolExecutor(int nThreads);
				static std::unique_ptr<Executor> newCachedThreadPoolExecutor();
				static std::unique_ptr<Executor> newWorkStealingPoolExecutor(int nThreads);
				static std::unique_ptr<Executor> newAsyncExecutor();
				static std::unique_ptr<Executor> newCoroutineExecutor();
				static std::unique_ptr<Executor> newFiberExecutor();
				static std::shared_ptr<std::unique_ptr<Executor>> getDefaultExecutor();
			};



		}
	}
}
