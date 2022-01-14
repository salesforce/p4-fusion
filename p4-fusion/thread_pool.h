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
	typedef std::function<void(P4API*)> Job;

	std::vector<std::thread> m_Threads;
	std::vector<std::exception_ptr> m_ThreadExceptions;
	std::vector<std::string> m_ThreadNames;
	std::vector<P4API> m_P4Contexts;
	unsigned m_count;
	std::atomic<unsigned> m_index { 0 };
	std::vector<notification_queue> m_q;

	bool m_HasShutDownBeenCalled;

	std::atomic<long> m_JobsProcessing;

private:
	void run(unsigned i);

public:
	static ThreadPool* GetSingleton();

	~ThreadPool();

	void Initialize(unsigned size);

	template <typename F>
	void AddJob(F&& function)
	{
		auto i = m_index++; // overflow of unsigned is well defined so no problem here
		for (unsigned n = 0; n != m_count; ++n)
		{
			if (m_q[(i + n) % m_count].try_push(std::forward<F>(function)))
				return;
		}
		m_q[i % m_count].push(std::forward<F>(function));
	}
	void Wait();
	void RaiseCaughtExceptions();
	void ShutDown();

	void Resize(int size);
	int GetThreadCount() const { return m_Threads.size(); }
};
