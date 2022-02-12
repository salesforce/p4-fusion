#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <condition_variable>

class P4API;

class NotificationQueue
{
	std::deque<std::function<void(P4API*)>> m_Queue;
	std::unique_ptr<std::mutex> m_Mutex;
	bool m_Done { false };
	std::unique_ptr<std::condition_variable> m_Ready;

public:
	NotificationQueue();

	void Done();

	bool TryPop(std::function<void(P4API*)>& x);

	bool Pop(std::function<void(P4API*)>& x);

	template <typename F>
	void Push(F&& f)
	{
		{
			std::unique_lock<std::mutex> lock { *m_Mutex };
			m_Queue.emplace_back(std::forward<F>(f));
		}
		m_Ready->notify_one();
	}

	template <typename F>
	bool TryPush(F&& f)
	{
		{
			std::unique_lock<std::mutex> lock { *m_Mutex, std::try_to_lock };
			if (!lock)
				return false;
			m_Queue.emplace_back(std::forward<F>(f));
		}
		m_Ready->notify_one();
		return true;
	}
};
