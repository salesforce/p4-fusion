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

#include "common.h"

class P4API;

class ThreadPool
{
	typedef std::function<void(P4API*)> Job;

	std::vector<std::thread> m_Threads;
	std::vector<std::exception_ptr> m_ThreadExceptions;
	std::vector<std::string> m_ThreadNames;
	std::vector<P4API> m_P4Contexts;

	std::deque<Job> m_Jobs;
	std::mutex m_JobsMutex;

	std::condition_variable m_CV;

	std::atomic<bool> m_ShouldStop;
	bool m_HasShutDownBeenCalled;

	std::atomic<long> m_JobsProcessing;

public:
	ThreadPool(int size);

	~ThreadPool();

	void Initialize(int size);
	void AddJob(Job function);
	void RaiseCaughtExceptions();
	void ShutDown();

	int GetThreadCount() const { return m_Threads.size(); }
private:
	void run(unsigned i);
};

namespace SystemThreadPool{
	ThreadPool& instance(int size = std::thread::hardware_concurrency()){
		static ThreadPool default_thread_pool(size);
		return default_thread_pool;
	}
}
