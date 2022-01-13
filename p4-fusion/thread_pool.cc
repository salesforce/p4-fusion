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

ThreadPool::ThreadPool(int size): m_HasShutDownBeenCalled(false), m_ShouldStop(false), m_JobsProcessing(0), m_P4Contexts(size){
  for(int i = 0; i < size; i++){
    m_ThreadExceptions.push_back(nullptr);
    m_ThreadNames.push_back("Worker #" + std::to_string(i));
    m_Threads.emplace_back([&, i]{ run(i); });
  }
  SUCCESS("Created " << size << " threads in thread pool");
}

void ThreadPool::run(unsigned i){
  MTR_META_THREAD_NAME(m_ThreadNames.at(i).c_str());
  P4API* localP4 = &m_P4Contexts[i];
  while(true){
    Job job;
    {
      std::unique_lock<std::mutex> lock(m_JobsMutex);
      m_CV.wait(lock, [this](){ return !m_Jobs.empty() || m_ShouldStop; });
      if(m_ShouldStop) break;
      job = m_Jobs.front();
      m_Jobs.pop_front();
    }
    try{
      job(localP4);
    }catch(const std::exception& e){
      m_ThreadExceptions[i] = std::current_exception();
    }
    m_JobsProcessing--;
  }
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
	{
		std::unique_lock<std::mutex> lock(m_JobsMutex);
		m_ShouldStop = true;
	}
	m_CV.notify_all();

	for (auto& thread : m_Threads)
	{
		thread.join();
	}
	SUCCESS("Thread pool shut down successfully");
}

ThreadPool::~ThreadPool()
{
	ShutDown();
}
