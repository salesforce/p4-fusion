#include "notification_queue.h"

NotificationQueue::NotificationQueue()
    : m_Ready(new std::condition_variable), m_Mutex(new std::mutex)
{
}

void NotificationQueue::Done()
{
	{
		std::unique_lock<std::mutex> lock { *m_Mutex };
		m_Done = true;
	}
	m_Ready->notify_all();
}

bool NotificationQueue::Pop(std::function<void(P4API*)>& x)
{
	std::unique_lock<std::mutex> lock { *m_Mutex };
	while (m_Queue.empty() && !m_Done)
		m_Ready->wait(lock);
	if (m_Queue.empty())
		return false;
	x = m_Queue.front();
	m_Queue.pop_front();
	return true;
}

bool NotificationQueue::TryPop(std::function<void(P4API*)>& x)
{
	std::unique_lock<std::mutex> lock { *m_Mutex, std::try_to_lock };
	if (!lock || m_Queue.empty())
		return false;
	x = m_Queue.front();
	m_Queue.pop_front();
	return true;
}
