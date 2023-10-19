/*
 * Copyright (c) 2022 Salesforce, Inc.
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
#include "p4_api.h"

class P4API;

class Thread
{
public:
	std::thread m_T;
	P4API m_P4;
	std::string m_Name;
};

typedef std::function<void(P4API*)> Job;

class ThreadPool
{
	std::vector<Thread> m_Threads;
	std::mutex m_ThreadExceptionsMutex;
	std::condition_variable m_ThreadExceptionCV;
	std::deque<std::exception_ptr> m_ThreadExceptions;

	std::deque<Job> m_Jobs;
	std::mutex m_JobsMutex;

	std::condition_variable m_CV;

	std::atomic<bool> m_ShouldStop;
	std::atomic<bool> m_HasShutDownBeenCalled;

public:
	ThreadPool(int size);
	ThreadPool() = delete;
	~ThreadPool();

	void AddJob(Job function);
	void RaiseCaughtExceptions();
	void ShutDown();
	int GetThreadCount() const { return m_Threads.size(); }
};
