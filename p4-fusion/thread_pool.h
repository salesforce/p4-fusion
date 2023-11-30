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
#include "git_api.h"
#include "thread.h"

class P4API;

typedef std::function<void(P4API&, GitAPI&)> Job;

class ThreadPool
{
private:
	mutable std::mutex m_ThreadMutex;
	std::vector<std::thread> m_Threads;
	std::mutex m_ThreadExceptionsMutex;
	std::condition_variable m_ThreadExceptionCV;
	std::deque<std::exception_ptr> m_ThreadExceptions;

	std::deque<Job> m_Jobs;
	std::mutex m_JobsMutex;

	std::condition_variable m_CV;

	std::once_flag m_ShutdownFlag;
	std::atomic<bool> m_HasShutDownBeenCalled;

	ThreadRAII exceptionHandlingThread;
	std::once_flag startExceptionHandlingThread_Flag;

	ThreadRAII signalHandlingThread;
	std::once_flag m_startSignalHandlingThread_Flag;
	std::once_flag m_shutdownSignalHandlingThread_Flag;

	void startExceptionHandlingThread();
	void startSignalHandlingThread();
	void shutdownSignalHandlingThread();

public:
	ThreadPool(int size, const std::string& repoPath, int tz);
	ThreadPool() = delete;
	~ThreadPool();

	void AddJob(Job&& function);
	void RaiseCaughtExceptions();
	void ShutDown();
	size_t GetThreadCount() const
	{
		std::lock_guard<std::mutex> lock(m_ThreadMutex);
		return m_Threads.size();
	}
};
