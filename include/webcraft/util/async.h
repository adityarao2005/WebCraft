#pragma once
#include <functional>
#include <coroutine>
#include <chrono>
#include <optional>
#include <future>
#include <thread>

namespace WebCraft {
	namespace Util {
		namespace Async {

			// Awaitable interface
			// Represents an operation that can be awaited in a coroutine
			template<typename T>
			struct Awaitable {
				// Should always return a boolean, checks if operation is complete
				// If true, the coroutine will not suspend & await_resume will be called immediately
				// If false, the coroutine will suspend & await_suspend will be called
				virtual bool await_ready() const = 0;
				// Called when the coroutine is suspended
				// The coroutine handle is passed to the function
				// The coroutine will be resumed when the operation is complete
				virtual auto await_suspend(std::coroutine_handle<> h) const = 0;
				// Called when the coroutine is resumed
				// Result of the operation is returned
				virtual T await_resume() const = 0;
			};

			// Represents an executor/scheduler which can schedule & dispatch tasks
			class Dispatcher;
			// Utility to provide main dispatchers
			class Dispatchers {
			public:
				// Returns the IO dispatcher - for IO bound tasks
				static std::shared_ptr<Dispatcher> IO();
				// Returns the main dispatcher - for UI bound tasks
				// Single thread executor will be used here
				static std::shared_ptr<Dispatcher> Main();
				// Returns the worker dispatcher - for CPU bound tasks
				// Thread pool will be used here
				static std::shared_ptr<Dispatcher> Worker();
			};

			// Awaiter for a function that returns a value
			// Tasks are only movable and non-copyable
			template <typename Result = void>
			class [[nodiscard]] Task {
			public:
				// FinalAwaiter is used to suspend the coroutine before it is destroyed
				struct FinalAwaiter {

					// Always returns false, so the coroutine is always suspended
					bool await_ready() const noexcept { return false; }

					// Called when the coroutine is suspended
					template <typename P>
					auto await_suspend(std::coroutine_handle<P> handle) noexcept
					{
						// Store the coroutine handle in the promise
						return handle.promise().continuation;
					}

					// Called when the coroutine is resumed
					void await_resume() const noexcept { }
				};

				// Promise type for the Task
				struct Promise {
					// Coroutine handle to resume when the operation is complete
					std::coroutine_handle<> continuation;
					// Result of the operation
					std::optional<Result> result;
					// Exception thrown by the operation
					std::exception_ptr exception;

					// Returns a Task object from the promise
					Task get_return_object()
					{
						// Create a Task object from the coroutine handle
						return Task{ std::coroutine_handle<Promise>::from_promise(*this) };
					}

					// Called when an exception is thrown
					void unhandled_exception() noexcept {
						exception = std::current_exception();
					}

					// Called when the operation returns a value
					void return_value(Result res) noexcept { result = std::move(res); }

					// Called when the coroutine is started
					std::suspend_always initial_suspend() noexcept { return {}; }
					// Called when the coroutine is destroyed
					FinalAwaiter final_suspend() noexcept { return {}; }
				};

				// Promise type for the Task
				using promise_type = Promise;

				// Constructor
				Task() = default;

				// Destructor
				~Task()
				{
					// If the coroutine handle is valid, destroy it
					if (handle_) {
						handle_.destroy();
					}
				}

				// Awaiter for the Task
				struct Awaiter {
					// Coroutine handle
					std::coroutine_handle<Promise> handle;

					// Returns true if the operation is complete
					bool await_ready() const noexcept { return !handle || handle.done(); }

					// Called when the coroutine is suspended
					auto await_suspend(std::coroutine_handle<> calling) noexcept
					{
						// Store the calling coroutine handle in the promise
						handle.promise().continuation = calling;
						// Return the coroutine handle
						return handle;
					}

					// Called when the coroutine is resumed
					template <typename T = Result>
						requires(std::is_same_v<T, void>)
					void await_resume() noexcept {
						Promise& promise = handle.promise();
						if (promise.exception) {
							std::rethrow_exception(promise.exception);
						}
					}

					// Called when the coroutine is resumed
					template <typename T = Result>
						requires(!std::is_same_v<T, void>)
					T&& await_resume() noexcept {
						// If an exception was thrown, rethrow it
						Promise& promise = handle.promise();
						if (promise.exception) {
							std::rethrow_exception(promise.exception);
						}

						// Return the result of the operation
						if (promise.result.has_value()) {
							return std::move(promise.result.value());
						}
						else {
							throw std::runtime_error("Promise value was not set");
						}
					}
				};

				// Checks if task is done
				bool done() const { return handle_.done(); }

				// Creates a continuation of the current task
				template<typename U, typename T = Result>
					requires(!std::is_same_v<T, void>)
				Task<U> then(std::function<U(T)> f) {
					// Ensure this Task completes and retrieve its result.
					T value = co_await *this;

					// Execute the function `f` with the result of this Task.
					U newValue = f(value);

					// Return a new Task containing the result of executing `f`.
					co_return newValue;
				}

				// Creates a continuation of the current task
				template<typename U, typename T = Result>
					requires(std::is_same_v<T, void>)
				Task<U> then(std::function<U()> f) {
					// Ensure this Task completes and retrieve its result.
					co_await *this;

					// Execute the function `f` with the result of this Task.
					U newValue = f();

					// Return a new Task containing the result of executing `f`.
					co_return newValue;
				}

				// Creates a task from a function
				template <typename T = Result>
				static Task<T> Create(std::function<T()> func) {
					co_return func();
				}

				// Co_await operator
				auto operator co_await() noexcept { return Awaiter{ handle_ }; }

			private:
				// Constructor
				explicit Task(std::coroutine_handle<Promise> handle)
					: handle_(handle)
				{
				}

				// Coroutine handle
				std::coroutine_handle<Promise> handle_;
			};

			// Specialization for Task<void>
			template <>
			struct Task<void>::Promise {
				// Continuation coroutine handle
				std::coroutine_handle<> continuation;
				// Exception thrown by the operation
				std::exception_ptr exception;


				// Returns a Task object from the promise
				Task get_return_object()
				{
					return Task{ std::coroutine_handle<Promise>::from_promise(*this) };
				}

				// Called when an exception is thrown
				void unhandled_exception() noexcept {
					exception = std::current_exception();
				}

				// Called when the operation returns a value
				void return_void() noexcept { }

				// Called when the coroutine is started
				std::suspend_always initial_suspend() noexcept { return {}; }
				// Called when the coroutine is destroyed
				FinalAwaiter final_suspend() noexcept { return {}; }
			};

			// Async macro
#define async(T) WebCraft::Util::Async::Task<T>

			// Awaiter for a function that returns void
			class Dispatcher {
			public:
				// Schedules a function to be executed on the dispatcher
				template <typename Func, typename... Args>
				std::future<std::invoke_result_t<Func, Args...>> schedule(Func&& func, Args&&... args) {
					// Deduce the return type of the function
					using ReturnType = typename std::invoke_result<Func, Args...>::type;

					// Create a packaged_task wrapping the function
					std::packaged_task<ReturnType()> task(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

					// Get the future before moving the task
					std::future<ReturnType> result = task.get_future();

					// Wrap the packaged_task in a function that can be queued
					std::function<bool()> wrapper = [task = std::move(task)]() mutable {
						task();
						return true;
						};

					// Queue the wrapper function for execution
					queue(wrapper);

					// Return the future to the caller
					return result;
				}

				void co_schedule(std::coroutine_handle<> handle) const {
					// Wrap the coroutine handle in a function that can be queued
					std::function<bool()> wrapper = [handle]() mutable {
						handle.resume();
						return true;
						};

					// Queue the wrapper function for execution
					queue(wrapper);
				}

				// Schedules a function to be executed on the dispatcher
				virtual void queue(std::function<bool()> func) const = 0;
			};

			namespace details {
				// Awaiter for a function that returns void
				Task<void> run_on_dispatcher(const std::shared_ptr<Dispatcher>& dispatcher);

				// Runs the tasks synchronously
				void run_sync(Task<void> task) {
					// Future type for the task
					struct Future {

						// Promise type for the future
						struct promise_type {
							Future get_return_object() { return {}; }
							std::suspend_never initial_suspend() { return {}; }
							std::suspend_never final_suspend() noexcept { return {}; }
							void unhandled_exception() {}
							void return_void() {}
						};

						// Awaiter for the future
						static Future create(Task<void> task) {
							// Runs the task on the main dispatcher
							co_await run_on_dispatcher(Dispatchers::Main());
							co_await task;
						}
					};

					// Run the task synchronously
					Future::create(task);
				}

				// Awaiter for a function that returns void
				auto operator co_await(std::chrono::milliseconds ms) {
					// Delay awaiter
					struct delay {
						// Delay duration
						std::chrono::milliseconds delay;

						// Returns false, so the coroutine is always suspended
						constexpr bool await_ready() const noexcept { return false; }
						// Called when the coroutine is suspended
						void await_suspend(std::coroutine_handle<> h) const noexcept {
							// Schedule a timer on the IO dispatcher
							auto start = std::chrono::steady_clock::now();
							// Queue the dispatch
							Dispatchers::IO()->queue([start, h, d = delay] {
								// If the delay has elapsed, resume the coroutine
								if (decltype(start)::clock::now() - start > d) {
									// Resume the coroutine
									h.resume();
									// Return true for continuation
									return true;
								}
								else {
									// Return false for continuation
									return false;
								}
								});
						}
						// Called when the coroutine is resumed
						void await_resume() const noexcept {}
					};
					// Return the delay awaiter
					return delay{ ms };
				}

				// Delays the coroutine for the specified duration
				Task<void> delay(std::chrono::milliseconds ms) {
					co_await ms;
				}

				// Runs the rest of the tasks on a dispatcher
				Task<void> run_on_dispatcher(const std::shared_ptr<Dispatcher>& dispatcher) {
					// Awaiter for running on the dispatcher
					struct run_on_dispatcher_awaiter {
						// Dispatcher reference
						const std::shared_ptr<Dispatcher>& dispatcher;

						// Returns false, so the coroutine is always suspended
						bool await_ready() const noexcept { return false; }

						// Called when the coroutine is suspended
						void await_suspend(std::coroutine_handle<> h) const noexcept {
							// Queue the dispatch on the dispatcher
							dispatcher->co_schedule(h);
						}

						// Called when the coroutine is resumed
						void await_resume() const noexcept {}
					};

					// Return the awaiter
					co_await run_on_dispatcher_awaiter{ dispatcher };
				}

				// Runs the rest of the tasks on a new thread
				Task<void> run_on_new_thread() {
					// Awaiter for running on the dispatcher
					struct run_on_new_thread_awaiter {
						// Returns false, so the coroutine is always suspended
						bool await_ready() const noexcept { return false; }

						// Called when the coroutine is suspended
						void await_suspend(std::coroutine_handle<> h) const noexcept {
							// Queue the dispatch on the dispatcher
							std::thread([&, h] {
								// Resume the coroutine
								h.resume();
								}).detach();
						}

						// Called when the coroutine is resumed
						void await_resume() const noexcept {}
					};

					// Return the awaiter
					co_await run_on_new_thread_awaiter{};
				}



			}

			template<std::movable T>
			class Generator
			{
			public:
				struct promise_type
				{
					Generator<T> get_return_object()
					{
						return Generator{ Handle::from_promise(*this) };
					}
					static std::suspend_always initial_suspend() noexcept
					{
						return {};
					}
					static std::suspend_always final_suspend() noexcept
					{
						return {};
					}
					std::suspend_always yield_value(T value) noexcept
					{
						current_value = std::move(value);
						return {};
					}
					// Disallow co_await in generator coroutines.
					void await_transform() = delete;
					[[noreturn]]
					static void unhandled_exception() { throw; }

					std::optional<T> current_value;
				};

				using Handle = std::coroutine_handle<promise_type>;

				explicit Generator(const Handle coroutine) :
					m_coroutine{ coroutine }
				{}

				Generator() = default;
				~Generator()
				{
					if (m_coroutine)
						m_coroutine.destroy();
				}

				Generator(const Generator&) = delete;
				Generator& operator=(const Generator&) = delete;

				Generator(Generator&& other) noexcept :
					m_coroutine{ other.m_coroutine }
				{
					other.m_coroutine = {};
				}
				Generator& operator=(Generator&& other) noexcept
				{
					if (this != &other)
					{
						if (m_coroutine)
							m_coroutine.destroy();
						m_coroutine = other.m_coroutine;
						other.m_coroutine = {};
					}
					return *this;
				}

				// Range-based for loop support.
				class Iter
				{
				public:
					void operator++()
					{
						m_coroutine.resume();
					}
					const T& operator*() const
					{
						return *m_coroutine.promise().current_value;
					}
					bool operator==(std::default_sentinel_t) const
					{
						return !m_coroutine || m_coroutine.done();
					}

					explicit Iter(const Handle coroutine) :
						m_coroutine{ coroutine }
					{}

				private:
					Handle m_coroutine;
				};

				Iter begin()
				{
					if (m_coroutine)
						m_coroutine.resume();
					return Iter{ m_coroutine };
				}

				std::default_sentinel_t end() { return {}; }

			private:
				Handle m_coroutine;
			};
		}

	}
}
