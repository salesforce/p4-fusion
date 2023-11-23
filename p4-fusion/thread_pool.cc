/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "thread_pool.h"
#include "common.h"
#include "p4_api.h"
#include "minitrace.h"
#include "git_api.h"

void ThreadPool::AddJob(Job&& function)
{
	// Fast path: if we're shutting down, don't even bother adding the job to
	// the queue.
	if (m_HasShutDownBeenCalled)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(m_JobsMutex);
	if (m_HasShutDownBeenCalled) // Check again, in case we shut down while waiting for the lock.
	{
		return;
	}

	m_Jobs.push_back(std::move(function));
	// Inform the next available job handler that there's new work.
	m_CV.notify_one();
}

void ThreadPool::RaiseCaughtExceptions()
{
	while (!m_HasShutDownBeenCalled)
	{
		std::unique_lock<std::mutex> lock(m_ThreadExceptionsMutex);
		m_ThreadExceptionCV.wait(lock);

		while (!m_ThreadExceptions.empty())
		{
			std::exception_ptr ex = m_ThreadExceptions.front();
			m_ThreadExceptions.pop_front();
			if (ex)
			{
				std::rethrow_exception(ex);
			}
		}
	}
}

void ThreadPool::ShutDown()
{
	std::lock_guard<std::mutex> shutdownLock(m_ShutdownMutex); // Prevent multiple threads from shutting down the pool.
	if (m_HasShutDownBeenCalled) // We've already shut down.
	{
		return;
	}

	// Signal that we want to shut down.
	{
		std::lock_guard<std::mutex> lock(m_JobsMutex);
		m_HasShutDownBeenCalled = true;
	}

	m_CV.notify_all(); // Tell all the worker threads to stop waiting for new jobs.
	m_ThreadExceptionCV.notify_all(); // Tell the exception handler to stop waiting for new exceptions.

	// Wait for all worker threads to finish, then release them.
	{
		std::lock_guard<std::mutex> lock(m_ThreadMutex);

		for (auto& thread : m_Threads)
		{
			thread.join();
		}

		m_Threads.clear();
	}

	// Clear the job queue.
	{
		std::lock_guard<std::mutex> lock(m_JobsMutex);
		m_Jobs.clear();
	}

	// Clear the exception queue.
	{
		std::lock_guard<std::mutex> lock(m_ThreadExceptionsMutex);
		m_ThreadExceptions.clear();
	}

	SUCCESS("Thread pool shut down successfully")
}

ThreadPool::ThreadPool(const int size, const std::string& repoPath, const int tz)
    : m_HasShutDownBeenCalled(false)
{
	// Initialize the thread handlers
	std::lock_guard<std::mutex> threadsLock(m_ThreadMutex);

	for (int i = 0; i < size; i++)
	{
		m_Threads.emplace_back([this, i, repoPath, tz]()
		    {
				// Add some human-readable info to the tracing.
				MTR_META_THREAD_NAME(("Worker #" + std::to_string(i)).c_str());

			    // Initialize p4 API.
			    P4API p4;
			    // We initialize a separate GitAPI per thread, otherwise
			    // internal locks will prevent the threads from working independently.
			    // We only write blob objects to the ODB, which according to libgit2/libgit2#2491
			    // is thread safe.
			    GitAPI git(repoPath, tz);

			    git.OpenRepository();

				// Job queue, we keep looking for new jobs until the shutdown
				// event.
				while (true)
				{
					Job job;
					{
						std::unique_lock<std::mutex> lock(m_JobsMutex);

						m_CV.wait(lock, [this]()
							{ return !m_Jobs.empty() || m_HasShutDownBeenCalled; });

						if (m_HasShutDownBeenCalled) // We're shutting down - exit.
						{
							break;
						}

						job = m_Jobs.front();
						m_Jobs.pop_front();
					}

					try
					{
						job(p4, git);
					}
					catch (const std::exception& e)
					{
						std::unique_lock<std::mutex> lock(m_ThreadExceptionsMutex);
						m_ThreadExceptions.push_back(std::current_exception());
						m_ThreadExceptionCV.notify_all();
					}
				} });
	}
}

ThreadPool::~ThreadPool()
{
	ShutDown();
}
