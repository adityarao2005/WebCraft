#pragma once

#include <webcraft/util/debug.h>
#include <webcraft/util/unit_test.h>
#include <iostream>
#include <coroutine>
#include <thread>
#include <queue>
#include <optional>
#include <functional>
#include <chrono>

std::queue<std::function<bool()>> task_queue;
using namespace std::chrono_literals;

struct sleep_l {
	sleep_l(int n) : delay{ n } {}

	constexpr bool await_ready() const noexcept { return false; }

	void await_suspend(std::coroutine_handle<> h) const noexcept {
		auto start = std::chrono::steady_clock::now();
		task_queue.push([start, h, d = delay] {
			if (decltype(start)::clock::now() - start > d) {
				h.resume();
				return true;
			}
			else {
				return false;
			}
			});
	}

	void await_resume() const noexcept  {}

	std::chrono::milliseconds delay;
};

template<typename T>
struct ret_l {
	T value;
	ret_l(T v) : value(v) { }

	constexpr bool await_ready() const noexcept { return true; }
	constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
	constexpr T await_resume() const noexcept { return value; }
};

struct Awaitable {
	// Should always return a boolean, checks if operation is complete
	// If true, the coroutine will not suspend & await_resume will be called immediately
	// If false, the coroutine will suspend & await_suspend will be called
	virtual bool await_ready() const = 0;
	// Called when the coroutine is suspended
	// The coroutine handle is passed to the function
	// The coroutine will be resumed when the operation is complete
	// If return value is void, coroutine is suspended and control is returned to the caller
	// If return value is bool and the return value is false, the coroutine is not suspended after all and await_resume is called immediately
	// If return value is bool and the return value is true, the coroutine is suspended and control is returned to the caller
	// If return value is std::coroutine_handle<>, the returned coroutine is resumed immediately after the current coroutine is suspended
	virtual void await_suspend(std::coroutine_handle<> h) const = 0;
	// Called when the coroutine is resumed
	// Result of the operation is returned
	virtual void await_resume() const = 0;
};

struct Task {
	struct promise_type {
		// Created at the beginning of the couroutine to create the return object
		// This is where you would typically start the asynchronous operation
		// It must return a Task object
		// Often wraps a std::coroutine_handle<> that refers to the coroutine
		Task get_return_object() { return {}; }
		// Coroutine should only define either a return_void() or a return_value() member function
		// return_value() should return the type of the value that the coroutine will return
		// return_void() should return the void
		// yield_value() should return the type of the value that the coroutine will return
		void return_void() {}

		// Called when the coroutine first starts, before any of the code in the coroutine is executed
		// This is where you would typically start the asynchronous operation
		// It must return an awaitable object that determines whether the coroutine should suspend or not
		// If the coroutine should not suspend, it should return std::suspend_never
		// If the coroutine should suspend, it should return std::suspend_always
		std::suspend_never initial_suspend() { return {}; }
		// Called when the coroutine is about to finish
		// This is where you would typically clean up the asynchronous operation
		// It must return an awaitable object that determines whether the coroutine should suspend or not
		// If the coroutine should not suspend, it should return std::suspend_never
		// If the coroutine should suspend, it should return std::suspend_always
		std::suspend_always final_suspend() noexcept { return {}; }
		// Called when an exception is thrown in the coroutine
		// This is where you would typically handle the exception
		// It must return void
		void unhandled_exception() {}
	};

	constexpr bool await_ready() const noexcept { return true; }
	constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
	constexpr void await_resume() const noexcept {}
};

template<typename T>
struct Task_0;

struct promise_base {
	std::coroutine_handle<> waiting;
	std::exception_ptr exception;

	void unhandled_exception() {
		exception = std::current_exception();
	}

	std::suspend_never initial_suspend() { return {}; }

	struct final_awaitable {
		bool await_ready() noexcept { return false; }

		template<typename PROMISE>
		std::coroutine_handle<> await_suspend(
			std::coroutine_handle<PROMISE> handle) noexcept {
			return handle.promise().m_continuation;
		}

		void await_resume() noexcept {}
	};

	final_awaitable final_suspend() noexcept { return {}; }
};

template<>
struct Task_0<void> {
	struct promise_type {
		

		Task_0<void> get_return_object() { return { Handle::from_promise(*this) }; }
		std::suspend_never initial_suspend() { return {}; }
		std::suspend_never final_suspend() { return {}; }
		void return_void() {}
		void unhandled_exception() {}
	};
	
	using Handle = std::coroutine_handle<promise_type>;

	constexpr bool await_ready() const noexcept { return true; }
	constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
	constexpr void await_resume() const noexcept {}
private:
	Handle m_coroutine;
	Task_0() = default;
	Task_0(const Handle coroutine) : m_coroutine{ coroutine } {}
	~Task_0() {
		if (m_coroutine)
			m_coroutine.destroy();
	}

	Task_0(const Task_0&) = delete;
	Task_0& operator=(const Task_0&) = delete;
	Task_0(Task_0&& other) noexcept : m_coroutine{ other.m_coroutine } {
		other.m_coroutine = {};
	}
	Task_0& operator=(Task_0&& other) noexcept {
		if (this != &other) {
			if (m_coroutine)
				m_coroutine.destroy();
			m_coroutine = other.m_coroutine;
			other.m_coroutine = {};
		}
		return *this;
	}
	
};

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



Task foo1() noexcept {
	std::cout << "1. hello from foo1" << std::endl;
	for (int i = 0; i < 10; ++i) {
		co_await sleep_l{ 10 };
		std::cout << i <<". hello from foo1" << std::endl;
	}
}

Task foo2() noexcept {
	std::cout << "1. hello from foo2" << std::endl;
	for (int i = 0; i < 10; ++i) {
		co_await sleep_l{ 10 };
		std::cout << i << ". hello from foo2" << std::endl;
	}
	std::string value = co_await ret_l<std::string>("hello");
	std::cout << value << std::endl;
}

inline auto operator co_await(std::string value) {
	return value;
}


const char* operator co_await(const char* value) {
	return value;
}


bool await_ready(const char*) noexcept { return false; }

bool await_suspend(
	std::coroutine_handle<> handle) noexcept {
	return false;
}

const char* await_resume(const char* bale) noexcept { return bale; }


Task foo3() {
	co_await foo1();
	co_await foo2();
	std::string hello = co_await "Hello";
}

template<std::integral T>
Generator<T> range(T first, const T last)
{
	while (first < last)
		co_yield first++;
}


class async_test : public WebCraft::Util::UnitTest {
public:
	void test_async() {
		foo3();

		while (!task_queue.empty()) {
			auto task = task_queue.front();
			if (!task()) {
				task_queue.push(task);
			}
			task_queue.pop();

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	void test_generator() {
		for (const char i : ::range(65, 91))
			std::cout << i << ' ';
		std::cout << '\n';
	}


	void Run() override {
		RUN_TEST(async_test, test_async);
		RUN_TEST(async_test, test_generator);
	}

	TO_STRING(web_math_test);

};
