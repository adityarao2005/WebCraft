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

class Executor {
public:
	# Method to schedule a task for execution
	virtual void Schedule(Executable* task);
	# Method to shutdown the executor
	virtual void Shutdown() {}
	virtual bool HasShutdown() { return true; }
}

class Executable {
public:
	virtual void Run() = 0;

	# Method to run the task synchronously
	void operator()() {
		Run();
	}

	# Method to run the task asynchronously
	void RunAsync() {
		GetExecutor().Schedule(this);
	}

	virtual Executor GetExecutor() = 0;
	virtual void SetExecutor(std::unique_ptr<Executor> executor) = 0;
}

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
}


class TaskUtils {
public:
	static Task<T> Create<Func, Args...>(Func&& func, Args... args);
	static Task<T> RunAsync<Func, Args...>(Func&& func, Args... args);
	static Task<void> Delay(int milliseconds);
};

class TaskBase<T> : Executable {
	std::function<T()> func;
	std::unique_ptr<std::exception> error;
	std::unique_ptr<Executor> executor;
	TaskStatus status;
	std::mutex mutex;
protected:
	TaskBase(std::function<T()> func);

	// Method to set the status of the task
	void SetStatus(TaskStatus status);

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

	// Method to get the executor to run the task
	Executor GetExecutor();

	// Method to set the executor to run the task
	void SetExecutor(std::unique_ptr<Executor> executor);

	// Method to get error if failed
	std::exception GetError();
}


MACRO async(T) Task<T>
MACRO await(T) t.GetResult()
```