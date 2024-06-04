#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace WebCraft {
	namespace Util {
		namespace Async {

			enum TaskStatus {
				Pending,
				Running,
				Completed,
				Failed,
				Cancelled,
				Yielded
			};


			template<typename T>
			class Task;
			class TaskUtils;
			class CancellationToken;
			template<typename T>
			class CancellableTask;
			class Executor;
			class Executable;

			class Executor {
			public:
				// Method to schedule a task for execution
				virtual void Schedule(std::shared_ptr<Executable> task) = 0;
				// Method to shutdown the executor
				virtual void Shutdown() = 0;
				virtual bool HasShutdown() = 0;
			};

			class Executable : public std::enable_shared_from_this<Executable> {
			public:
				virtual void Run() = 0;

				// Method to run the task synchronously
				void operator()() {
					Run();
				}

				// Method to run the task asynchronously
				void RunAsync() {
					GetExecutor().Schedule(shared_from_this());
				}

				virtual Executor& GetExecutor() = 0;
				virtual void SetExecutor(std::shared_ptr<Executor> executor) = 0;
			};

			class Executors {
			private:
				static std::unique_ptr<Executor> defaultExecutor;
			public:
				static std::unique_ptr<Executor> newSingleThreadExecutor();
				static std::unique_ptr<Executor> newFixedThreadPoolExecutor(int nThreads);
				static std::unique_ptr<Executor> newCachedThreadPoolExecutor();
				static std::unique_ptr<Executor> newWorkStealingPoolExecutor(int nThreads);
				static std::unique_ptr<Executor> newThreadPerTaskExecutor();
				static std::unique_ptr<Executor> newCoroutineExecutor();
				static std::unique_ptr<Executor> newFiberExecutor();
				static Executor* getDefaultExecutor();
				static void setDefaultExecutor(std::unique_ptr<Executor> executor);
			};

			class TaskUtils {
			public:

				template<typename T, typename Func, typename... Args>
				static Task<T> Create(Func&& func, Args... args);

				template<typename T, typename Func, typename... Args>
				static Task<T> RunAsync(Func&& func, Args... args);
				static Task<void> Delay(int milliseconds);

			};

			template<typename T>
			class TaskBase : public Executable {
			private:
				std::function<T()> func;
				std::unique_ptr<std::exception> error;
				std::unique_ptr<Executor> executor;
				TaskStatus status;
			protected:
				std::mutex mutex;
				TaskBase(std::function<T()> func) : func(func), error(nullptr), status(Pending), executor(Executors::getDefaultExecutor()) {}

				// Method to set the status of the task
				void SetStatus(TaskStatus status) {
					std::lock_guard<std::mutex> lock(mutex);
					this->status = status;
				}

				void SetError(const std::exception& e) {
					std::lock_guard<std::mutex> lock(mutex);
					error = std::make_unique<std::exception>(e);
				}

				std::function<T()> GetFunc() {
					return func;
				}

			public:

				// Method to get the status of the task
				TaskStatus GetStatus() {
					std::lock_guard<std::mutex> lock(mutex);
					return status;
				}

				Executor& GetExecutor() {
					return *executor;
				}
				void SetExecutor(std::shared_ptr<Executor> executor) {
					this->executor = executor;
				}

				// Method to get error if failed
				std::exception& GetError() {
					std::lock_guard<std::mutex> lock(mutex);
					return *error;
				}

			};

			template<class T>
			class Task : public TaskBase<T> {
			private:
				friend class TaskUtils;
				std::unique_ptr<T> result;

				// Constructor which takes in a lambda function
				Task(std::function<T()> func) : TaskBase(func) {}


				T _GetResult() {
					std::lock_guard<std::mutex> lock(this->mutex);
					return *result;
				}

				void _SetResult(std::unique_ptr<T> result) {
					std::lock_guard<std::mutex> lock(this->mutex);
					this->result = result;
				}
			public:
				// Method to run the task
				void Run() override {
					try {
						SetStatus(Running);
						_SetResult(std::make_unique<T>(GetFunc()()));
						SetStatus(Completed);
					}
					catch (std::exception& e) {
						SetError(e);
						SetStatus(Failed);
					}
				}

				// Method to get the result of the task
				// Awaits the task to complete if not completed
				T GetResult() {
					if (GetStatus() == Pending) {
						RunAsync();
						SetStatus(Running);
					}
					if (GetStatus() == Running) {
						while (GetStatus() != Completed && GetStatus() != Failed) {
							std::this_thread::yield();
						}
					}
					if (GetStatus() == Failed) {
						throw GetError();
					}
					return *_GetResult();

				}
				// Continuation method to run another task after this task completes
					// enable only if T is not void
				template<typename V>
				Task<V> Then(std::function<V(T)> func) {
					return TaskUtils::Create<V>([this, func]() {
						return func(GetResult());
						});
				}
			};

			template<>
			class Task<void> : public TaskBase<void> {
			private:
				friend class TaskUtils;

				// Constructor which takes in a lambda function
				Task(std::function<void()> func) : TaskBase(func) {}

			public:
				// Method to run the task
				virtual void Run() override {
					try {
						SetStatus(Running);
						GetFunc()();
						SetStatus(Completed);
					}
					catch (std::exception& e) {
						SetError(e);
						SetStatus(Failed);
					}
				}

				// Method to get the result of the task
				// Awaits the task to complete if not completed
				void GetResult() {
					if (GetStatus() == Pending) {
						RunAsync();
					}
					if (GetStatus() == Running) {
						while (GetStatus() != Completed && GetStatus() != Failed) {
							std::this_thread::yield();
						}
					}
					if (GetStatus() == Failed) {
						throw GetError();
					}

				}

			};

			template<typename T, typename Func, typename... Args>
			Task<T> TaskUtils::Create(Func&& func, Args... args) {
				return Task<T>([func, args...]() {
					return func(args...);
					});
			}

			template<typename T, typename Func, typename... Args>
			Task<T> TaskUtils::RunAsync(Func&& func, Args... args) {
				Task<T> task = Create<T>(func, args...);
				task.RunAsync();
				return task;
			}

			Task<void> TaskUtils::Delay(int milliseconds) {
				return Task<void>([milliseconds]() {
					std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
					});
			}


			/*
						class CancellationToken {
						private:
							bool cancelled = false;
						public:
							bool IsCancelled();
						};

						template<typename T>
						class CancellableTask : public Task<T> {
						private:
							// Constructor which takes in a lambda function
							CancellableTask(std::function<T(CancellationToken)> func);

						public:
							// Method to cancel the task
							void Cancel();

							// Runs the task
							void Run() override;

							// Method to get the status of the task
							TaskStatus GetStatus();
						};

						class YieldingTask<T> : public Task<T> {
						private:
							// Constructor which takes in a lambda function
								YieldingTask<T>(std::function<T()> func);

						public:
							// Macros to yield and begin / end coroutine
								// use black magic like duff's device to implement this
								MACRO YIELD;
							MACRO BEGIN_COROUTINE;
							MACRO END_COROUTINE;

							// Method to get the status of the task
								TaskStatus GetStatus();

							// Method to get the state of the task
								// Used to resume the task from the last state
							//	State GetState();
						}; */

#define async(T) Task<T>
#define await(t) t.GetResult()
		}
	}
}
