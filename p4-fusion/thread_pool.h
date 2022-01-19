/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <thread>
#include <deque>
#include <functional>
#include <atomic>
#include <condition_variable>
#include "notification_queue.h"

#include "common.h"

class P4API;

class ThreadPool
{
	std::vector<std::thread> m_Threads;
	std::vector<std::exception_ptr> m_ThreadExceptions;
	std::vector<std::string> m_ThreadNames;
	std::vector<P4API> m_P4Contexts;
	unsigned m_Count;
	std::atomic<unsigned> m_Index { 0 };
	std::vector<NotificationQueue> m_TaskQueue;

	bool m_HasShutDownBeenCalled;

private:
	void run(unsigned i);

public:
	static ThreadPool* GetSingleton();

	~ThreadPool();

	void Initialize(unsigned size);

	template <typename F>
	void AddJob(F&& function)
	{
		auto i = m_Index++; // overflow of unsigned is well defined so no problem here
		for (unsigned n = 0; n != m_Count; ++n)
		{
			if (m_TaskQueue[(i + n) % m_Count].TryPush(std::forward<F>(function)))
				return;
		}
		m_TaskQueue[i % m_Count].Push(std::forward<F>(function));
	}
	void RaiseCaughtExceptions();
	void ShutDown();

	void Resize(int size);
	int GetThreadCount() const { return m_Threads.size(); }
};
