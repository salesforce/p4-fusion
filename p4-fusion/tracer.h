/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <future>
#include <filesystem>
#include "minitrace.h"

/*
 * Tracer wraps the minitrace library, making sure that we regularly flush and
 * finalize traces on shutdown.
 */
class Tracer
{
private:
	std::atomic_bool shouldStop;
	std::future<void> flusher;

public:
	Tracer(const std::string& traceFileDir, int flushRate)
	    : shouldStop(false)
	{
#if !(MTR_ENABLED)
		return;
#endif

		// Ensure the path exists.
		std::filesystem::create_directories(traceFileDir);
		const std::string tracePath = (traceFileDir + (traceFileDir.back() == '/' ? "" : "/") + "trace.json");

		mtr_init(tracePath.c_str());
		MTR_META_PROCESS_NAME("p4-fusion");
		MTR_META_THREAD_NAME("Main Thread");
		MTR_META_THREAD_SORT_INDEX(0);

		SUCCESS("Set up tracer to write profile to " << tracePath)

		flusher = std::async(std::launch::async, [flushRate, this]()
		    {
			while (true)
			{
				// Check every 100ms if the thread should be stopped.
				// flushRate is in seconds, so flushRate times 10 is
				// in 100 ms steps.
				int loops = flushRate * 10;
				for (int i = 0; i < loops; i++)
				{
					if (shouldStop)
					{
						return;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				mtr_flush();
			} });
	}
	~Tracer()
	{
#if !(MTR_ENABLED)
		return;
#endif

		// Signal to flusher that it should stop working.
		shouldStop = true;
		// Wait for flusher to shut down.
		flusher.get();
		// Do a final flush.
		mtr_flush();
		// And finally shut down the mtr library.
		mtr_shutdown();

		SUCCESS("Tracer shut down successfully")
	}
};
