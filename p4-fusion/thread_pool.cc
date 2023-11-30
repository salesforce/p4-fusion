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
#include "thread.h"
#include "git_api.h"
#include "signal.h"

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
	auto stop = [this]()
	{
		// Signal that we want to shut down.
		{
			std::lock_guard<std::mutex> lock(m_JobsMutex);
			m_HasShutDownBeenCalled = true;
		}

		m_CV.notify_all(); // Tell all the worker threads to stop waiting for new jobs.

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

		SUCCESS("Thread pool shut down successfully")

		// Now as the last step, stop the exception handling thread:

		m_ThreadExceptionCV.notify_all(); // Tell the exception handler to stop waiting for new exceptions.

		// Clear the exception queue.
		{
			std::lock_guard<std::mutex> lock(m_ThreadExceptionsMutex);
			m_ThreadExceptions.clear();
		}

		// Shutdown the signal handler thread.
		shutdownSignalHandlingThread();
	};

	std::call_once(m_ShutdownFlag, stop);
}

ThreadPool::ThreadPool(const int size, const std::string& repoPath, const int tz)
    : m_HasShutDownBeenCalled(false)
{

	startSignalHandlingThread();
	startExceptionHandlingThread();

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

void ThreadPool::startSignalHandlingThread()
{
	auto start = [this]()
	{
		// Block signals from being handled by the main thread, and all future threads.
		//
		// - SIGINT, SIGTERM, SIGHUP: thread pool will be shutdown and std::exit will be called
		// - SIGUSR1: only sent by ~SignalHandler() to tell the signal handler thread to exit
		sigset_t blockedSignals;
		sigemptyset(&blockedSignals);
		for (auto sig : { SIGINT, SIGTERM, SIGHUP, SIGUSR1 })
		{
			sigaddset(&blockedSignals, sig);
		}

		int rc = pthread_sigmask(SIG_BLOCK, &blockedSignals, nullptr);
		if (rc != 0)
		{
			std::ostringstream oss;
			oss << "(signal handler) failed to block signals: (" << errno << ") " << strerror(errno);
			throw std::runtime_error(oss.str());
		}

		sigset_t signalsToWaitOn = blockedSignals;
		// Spawn a thread to handle signals.
		// The thread will block and wait for signals to arrive and then shutdown the thread pool, unless it receives SIGUSR1,
		// in which case it will just exit (since main() is handling the shutdown).
		//
		// Using a separate thread for purely signal handling allows us to use non-reentrant functions
		// (such as std::cout, condition variables, etc.) in the signal handler.
		signalHandlingThread = ThreadRAII(std::thread(
		    [signalsToWaitOn, this]()
		    {
			    // Wait for signals to arrive.
			    int sig;
			    int rc = sigwait(&signalsToWaitOn, &sig);
			    if (rc != 0)
			    {
				    ERR("(signal handler) failed to wait for signals: (" << errno << ") " << strerror(errno))
				    ShutDown();
				    std::exit(errno);
			    }

			    // Did main() tell us to shutdown?
			    if (sig == SIGUSR1)
			    {
				    // Yes, so no need to print anything - just exit.
				    return;
			    }

			    // Otherwise, we received a signal from the OS - print a message and shutdown.
			    if (!sigismember(&signalsToWaitOn, sig))
			    {
				    ERR("(signal handler): WARNING: received signal (" << sig << ") \"" << strsignal(sig) << "\" that is not blocked, this should not happen and indicates a logic error in the signal handler.")
			    }

			    ERR("(signal handler) received signal (" << sig << ") \"" << strsignal(sig) << "\", shutting down")
			    ShutDown();
			    std::exit(sig);
		    }));
	};

	std::call_once(m_startSignalHandlingThread_Flag, start);
}

void ThreadPool::shutdownSignalHandlingThread()
{
	auto stop = [this]()
	{
		auto errcode = pthread_kill(signalHandlingThread.get().native_handle(), SIGUSR1);
		if (errcode != 0)
		{
			ERR("(signal handler) failed to shut down signal handling thread: (" << errcode << ") " << strerror(errcode))
			return;
		}

		SUCCESS("Signal handler shut down successfully")
		return;
	};

	return std::call_once(m_shutdownSignalHandlingThread_Flag, stop);
}

void ThreadPool::startExceptionHandlingThread()
{
	auto start = [this]()
	{
		exceptionHandlingThread = ThreadRAII(std::thread([this]()
		    {
			// See if the threadpool encountered any exceptions.
			try
			{
				RaiseCaughtExceptions();
			}
			catch (const std::exception& e)
			{
				// This is unrecoverable
				ERR("Threadpool encountered an exception: " << e.what())
				ShutDown();
				std::exit(1);
			}
			SUCCESS("Exception handler finished") }));
	};

	std::call_once(startExceptionHandlingThread_Flag, start);
}

ThreadPool::~ThreadPool()
{
	ShutDown();
}
