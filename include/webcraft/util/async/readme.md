# Asynchronous Programming

## What is Asynchronous Programming?

Asynchronous programming is the ability to execute multiple tasks concurrently. This is a very important concept in programming, as it allows you to write code that can perform multiple tasks at the same time. This is especially useful when you need to perform tasks that are time-consuming, such as reading data from a file, making a network request, or performing a complex calculation.

## Why is Asynchronous Programming Important?

Asynchronous programming is important because it allows you to write code that is more efficient and responsive. By executing multiple tasks concurrently, you can make better use of your computer's resources and reduce the amount of time it takes to complete a task. This can be especially useful in web development, where you need to perform tasks such as fetching data from a server, updating the user interface, or handling user input.

## How Does Asynchronous Programming Work In Our Framework?

Although many languages such as JavaScript, Python, and C# have built-in support for asynchronous programming through features such as async/await. C++ natively does not have support for this feature unless you are using C++ 20. However, in order to have this library gain attention for most C++ developers, we have implemented a custom asynchronous programming library that allows you to write asynchronous code in C++.

This is done through Tasks, Executors, and ... Macros as replacement for async/await keyworks.

## What is a Task?

A Task is a unit of work that can be executed asynchronously. It represents a single operation that can be executed concurrently with other tasks. Tasks can be created using the Task class, which takes a lambda function as an argument. The lambda function represents the code that will be executed asynchronously. Tasks can be scheduled for execution using an Executor.

There are 3 types of tasks: Regular Tasks, Cancellable Tasks, Yielding Tasks.

## What is an Executor?

An Executor is an object that is responsible for executing tasks. It is used to schedule tasks for execution and manage the resources needed to run them.

## Pseudo-Code Implementation

```
enum TaskStatus {
	Pending,
	Running,
	Completed,
	Failed,
	Cancelled,
	Yielded
};

class Task<T> {
	private:
		# Constructor which takes in a lambda function
		Task<T>(std::function<T()> func);

	public:
		# Static method to create a task
		static Task<T> Create<Args...>(std::function<T(Args...)> func, Args... args);

		# Method to run the task
		void Run();

		# Method to run the task asynchronously
		void RunAsync();

		# Overloaded operator to run the task
		void operator()(); 

		# Method to get the result of the task
		# Awaits the task to complete if not completed
		T GetResult();

		# Method to get the status of the task
		TaskStatus GetStatus();

		# Method to get error if failed
		std::unique_ptr<std::exception> GetError();

		# Continuation method to run another task after this task completes
		## enable only if T is not void
		Task<V> Then<V>(std::function<V(T)> func);

		# Default executor is the one returned by Executors::getDefaultExecutor()
		# Gets the executor to run the task
		std::shared_ptr<std::unique_ptr<Executor>> GetExecutor();

		# Sets the executor to run the task
		void SetExecutor(std::unique_ptr<Executor> executor);

		static Task<void> Delay(int milliseconds);
};

class CancellableTask<T> : public Task<T> {
	private:
		# Constructor which takes in a lambda function
		CancellableTask<T>(std::function<T(CancellationToken)> func);

	public:

		# Static method to create a task
		static CancellableTask<T> Create<Args...>(std::function<T(CancellationToken, Args...)> func, Args... args);

		# Method to cancel the task
		void Cancel();

		# Method to get the status of the task
		TaskStatus GetStatus();
};

class YieldingTask<T> : public Task<T> {
	private:
		# Constructor which takes in a lambda function
		YieldingTask<T>(std::function<T()> func);

	public:
		# Macros to yield and begin/end coroutine
		# use black magic like duff's device to implement this
		MACRO YIELD;
		MACRO BEGIN_COROUTINE;
		MACRO END_COROUTINE;

		# Method to get the status of the task
		TaskStatus GetStatus();

		# Method to get the state of the task
		# Used to resume the task from the last state
		State GetState();
};

class Executor {
public:
	# Method to schedule a task for execution
	virtual void Schedule(std::shared_ptr<Task<T>> task) = 0;
	virtual void Shutdown() = 0;
	virtual bool IsShutdown() = 0;
};

class Executors {
public:
	static std::unique_ptr<Executor> newSingleThreadExecutor();
	static std::unique_ptr<Executor> newFixedThreadPoolExecutor(int nThreads);
	static std::unique_ptr<Executor> newCachedThreadPoolExecutor();
	static std::unique_ptr<Executor> newWorkStealingPoolExecutor(int nThreads);
	static std::unique_ptr<Executor> newAsyncExecutor();
	static std::unique_ptr<Executor> newCoroutineExecutor();
	static std::unique_ptr<Executor> newFiberExecutor();
	static std::shared_ptr<std::unique_ptr<Executor>> getDefaultExecutor();
}

MACRO ASYNC(T) Task<T>
MACRO AWAIT(T) t.GetResult()
```