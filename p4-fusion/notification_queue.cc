#include "notification_queue.h"

void notification_queue::done()
{
	{
		std::unique_lock<std::mutex> lock { mutex_ };
		done_ = true;
	}
	ready_.notify_all();
}

bool notification_queue::pop(std::function<void(P4API*)>& x)
{
	std::unique_lock<std::mutex> lock { mutex_ };
	while (q_.empty() && !done_)
		ready_.wait(lock);
	if (q_.empty())
		return false;
	x = q_.front();
	q_.pop_front();
	return true;
}

bool notification_queue::try_pop(std::function<void(P4API*)>& x)
{
	std::unique_lock<std::mutex> lock { mutex_, std::try_to_lock };
	if (!lock || q_.empty())
		return false;
	x = q_.front();
	q_.pop_front();
	return true;
}
