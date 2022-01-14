/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "thread_pool.h"

#include "common.h"

#include "utils/arguments.h"
#include "p4_api.h"

#include "minitrace.h"

ThreadPool* ThreadPool::GetSingleton()
{
	static ThreadPool singleton;
	return &singleton;
}

void ThreadPool::Wait()
{
	while (true)
	{
		if (m_JobsProcessing == 0)
		{
			return;
		}
	}
}

void ThreadPool::RaiseCaughtExceptions()
{
	for (auto& exceptionPtr : m_ThreadExceptions)
	{
		if (exceptionPtr)
		{
			std::rethrow_exception(exceptionPtr);
		}
	}
}

void ThreadPool::ShutDown()
{
	if (m_HasShutDownBeenCalled)
	{
		return;
	}
	m_HasShutDownBeenCalled = true;

	for (auto& q : m_TaskQueue)
	{
		q.Done();
	}
	for (auto& thread : m_Threads)
	{
		thread.join();
	}

	m_Threads.clear();
	m_ThreadExceptions.clear();
	m_ThreadNames.clear();

	SUCCESS("Thread pool shut down successfully");
}

void ThreadPool::Resize(int size)
{
	ShutDown();
	Initialize(size);
}

void ThreadPool::Initialize(unsigned size)
{
	m_Count = size;
	m_Index = 0;
	m_TaskQueue.clear();
	m_HasShutDownBeenCalled = false;
	m_JobsProcessing = 0;

	m_P4Contexts.resize(size);

	for (unsigned i = 0u; i < size; i++)
	{
		m_TaskQueue.push_back(NotificationQueue());
		m_ThreadExceptions.push_back(nullptr);
		m_ThreadNames.push_back("Worker #" + std::to_string(i));
		m_Threads.emplace_back([this, i]()
		    {
			    MTR_META_THREAD_NAME(m_ThreadNames.at(i).c_str());
			    run(i); });
	}

	SUCCESS("Created " << size << " threads in thread pool");
}

void ThreadPool::run(unsigned i)
{
	while (true)
	{
		std::function<void(P4API*)> f;
		for (unsigned n = 0; n != m_Count; ++n)
		{
			if (!m_TaskQueue[(i + n) % m_Count].TryPop(f))
				break;
		}
		if (!f && !m_TaskQueue[i].Pop(f))
			break;
		try
		{
			f(&m_P4Contexts[i]);
		}
		catch (const std::exception& e)
		{
			m_ThreadExceptions[i] = std::current_exception();
		}
		m_JobsProcessing--;
	}
}

ThreadPool::~ThreadPool()
{
	ShutDown();
}
