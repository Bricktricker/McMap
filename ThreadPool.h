#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <iostream>
#include <future>
#include <vector>
#include <thread>
#include <queue>

class ThreadPool
{
public:
	using Task = std::function<void()>;

	explicit ThreadPool(const std::size_t numThreads)
	{
		start(numThreads);
	}

	~ThreadPool()
	{
		stop();
	}

	size_t size() const noexcept
	{
		return mThreads.size();
	}

	template<class T, typename... Arguments>
	auto enqueue(T task, Arguments... args) -> std::future<decltype(task(args...))>
	{
		typedef decltype(task(args...)) Ret;
		auto wrapper = std::make_shared<std::packaged_task<Ret ()>>(std::move(std::bind(task, args...)));

		{
			std::unique_lock<std::mutex> lock{ mEventMutex };
			mTasks.emplace([=] {
				(*wrapper)();
			});
		}

		mEventVar.notify_one();
		return wrapper->get_future();
	}

private:
	std::vector<std::thread> mThreads;

	std::condition_variable mEventVar;

	std::mutex mEventMutex;
	bool mStopping = false;

	std::queue<Task> mTasks;

	void start(const std::size_t numThreads)
	{
		for (auto i = 0u; i < numThreads; ++i)
		{
			mThreads.emplace_back([=] {
				while (true)
				{
					Task task;

					{
						std::unique_lock<std::mutex> lock{ mEventMutex };

						mEventVar.wait(lock, [=] { return mStopping || !mTasks.empty(); });

						if (mStopping && mTasks.empty())
							break;

						task = std::move(mTasks.front());
						mTasks.pop();
					}

					task();
				}
			});
		}
	}

	void stop() noexcept
	{
		{
			std::unique_lock<std::mutex> lock{ mEventMutex };
			mStopping = true;
		}

		mEventVar.notify_all();

		for (auto &thread : mThreads)
			thread.join();
	}
};

#endif
