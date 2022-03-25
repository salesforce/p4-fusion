/*
 * Copyright (c) 2022, salesforce.com, inc.
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

void ThreadPool::AddJob(Job function)
{
	{
		std::unique_lock<std::mutex> lock(m_JobsMutex);
		m_Jobs.push_back(function);
		m_JobsProcessing++;
	}
	m_CV.notify_one();
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
	std::unique_lock<std::mutex> lock(m_ThreadExceptionsMutex);

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

	{
		std::unique_lock<std::mutex> lock(m_JobsMutex);
		m_ShouldStop = true;
	}
	m_CV.notify_all();

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

void ThreadPool::Initialize(int size)
{
	m_HasShutDownBeenCalled = false;
	m_ShouldStop = false;
	m_JobsProcessing = 0;

	m_P4Contexts.resize(size);

	for (int i = 0; i < size; i++)
	{
		m_ThreadExceptions.push_back(nullptr);
		m_ThreadNames.push_back("Worker #" + std::to_string(i));
		m_Threads.push_back(std::thread([this, i]()
		    {
			    MTR_META_THREAD_NAME(m_ThreadNames.at(i).c_str());

			    P4API* localP4 = &m_P4Contexts[i];

			    while (true)
			    {
				    Job job;
				    {
					    std::unique_lock<std::mutex> lock(m_JobsMutex);

					    m_CV.wait(lock, [this]()
					        { return !m_Jobs.empty() || m_ShouldStop; });

					    if (m_ShouldStop)
					    {
						    break;
					    }

					    job = m_Jobs.front();
					    m_Jobs.pop_front();
				    }

				    try
				    {
					    job(localP4);
				    }
				    catch (const std::exception& e)
				    {
					    std::unique_lock<std::mutex> lock(m_ThreadExceptionsMutex);

					    m_ThreadExceptions[i] = std::current_exception();
				    }
				    m_JobsProcessing--;
			    }
		    }));
	}
}

ThreadPool::~ThreadPool()
{
	ShutDown();
}
