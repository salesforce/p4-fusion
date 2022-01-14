#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <condition_variable>

class P4API;

class notification_queue
{
	std::deque<std::function<void(P4API*)>> q_;
	std::mutex mutex_;
	bool done_ { false };
	std::condition_variable ready_;

public:
	void done();

	bool try_pop(std::function<void(P4API*)>& x);

	bool pop(std::function<void(P4API*)>& x);

	template <typename F>
	void push(F&& f)
	{
		{
			std::unique_lock<std::mutex> lock { mutex_ };
			q_.emplace_back(std::forward<F>(f));
		}
		ready_.notify_one();
	}

	template <typename F>
	bool try_push(F&& f)
	{
		{
			std::unique_lock<std::mutex> lock { mutex_, std::try_to_lock };
			if (!lock)
				return false;
			q_.emplace_back(std::forward<F>(f));
		}
		ready_.notify_one();
		return true;
	}
};
