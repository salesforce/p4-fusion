/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include <thread>
#include <map>
#include <atomic>
#include <mutex>
#include <sstream>
#include <typeinfo>
#include <signal.h>

#include "common.h"

#include "utils/timer.h"
#include "utils/arguments.h"

#include "thread_pool.h"
#include "p4_api.h"
#include "git_api.h"

#include "p4/p4libs.h"
#include "minitrace.h"

int Main(int argc, char** argv)
{
	PRINT("Running p4-fusion from: " << argv[0]);

	Timer programTimer;

	Arguments::GetSingleton()->RequiredParameter("--path", "P4 depot path to convert to a Git repo");
	Arguments::GetSingleton()->RequiredParameter("--src", "Local relative source path with P4 code. Git repo will be created at this path. This path should be empty before running p4-fusion.");
	Arguments::GetSingleton()->RequiredParameter("--port", "Specify which P4PORT to use.");
	Arguments::GetSingleton()->RequiredParameter("--user", "Specify which P4USER to use. Please ensure that the user is logged in.");
	Arguments::GetSingleton()->RequiredParameter("--client", "Name/path of the client workspace specification.");
	Arguments::GetSingleton()->RequiredParameter("--lookAhead", "How many CLs in the future, at most, shall we keep downloaded by the time it is to commit them?");
	Arguments::GetSingleton()->OptionalParameter("--networkThreads", std::to_string(std::thread::hardware_concurrency()), "Specify the number of threads in the threadpool for running network calls. Defaults to the number of logical CPUs.");
	Arguments::GetSingleton()->OptionalParameter("--printBatch", "1", "Specify the p4 print batch size.");
	Arguments::GetSingleton()->OptionalParameter("--maxChanges", "-1", "Specify the max number of changelists which should be processed in a single run. -1 signifies unlimited range.");
	Arguments::GetSingleton()->OptionalParameter("--retries", "10", "Specify how many times a command should be retried before the process exits in a failure.");
	Arguments::GetSingleton()->OptionalParameter("--refresh", "100", "Specify how many times a connection should be reused before it is refreshed.");
	Arguments::GetSingleton()->OptionalParameter("--fsyncEnable", "false", "Enable fsync() while writing objects to disk to ensure they get written to permanent storage immediately instead of being cached. This is to mitigate data loss in events of hardware failure.");
	Arguments::GetSingleton()->OptionalParameter("--includeBinaries", "false", "Do not discard binary files while downloading changelists.");
	Arguments::GetSingleton()->OptionalParameter("--flushRate", "1000", "Rate at which profiling data is flushed on the disk.");

	Arguments::GetSingleton()->Initialize(argc, argv);
	if (!Arguments::GetSingleton()->IsValid())
	{
		PRINT("Usage:" + Arguments::GetSingleton()->Help());
		return 1;
	}

	const std::string depotPath = Arguments::GetSingleton()->GetDepotPath();
	const std::string srcPath = Arguments::GetSingleton()->GetSourcePath();
	const bool fsyncEnable = Arguments::GetSingleton()->GetFsyncEnable() != "false";
	const bool includeBinaries = Arguments::GetSingleton()->GetIncludeBinaries() != "false";
	const int maxChanges = std::atoi(Arguments::GetSingleton()->GetMaxChanges().c_str());
	const int flushRate = std::atoi(Arguments::GetSingleton()->GetFlushRate().c_str());

	if (!P4API::InitializeLibraries())
	{
		return 1;
	}

	P4API::P4PORT = Arguments::GetSingleton()->GetPort();
	P4API::P4USER = Arguments::GetSingleton()->GetUsername();
	P4API::P4CLIENT = Arguments::GetSingleton()->GetClient();
	P4API::ClientSpec = P4API().Client().GetClientSpec();

	PRINT("Updated client workspace view " << P4API::ClientSpec.client << " with " << P4API::ClientSpec.mapping.size() << " mappings");

	P4API p4;

	if (!p4.IsDepotPathValid(depotPath))
	{
		ERR("Depot path should begin with \"//\" and end with \"/...\". Please pass in the proper depot path and try again.");
		return 1;
	}

	if (!p4.IsFileUnderClientSpec(depotPath))
	{
		ERR("The depot path specified is not under the " << P4API::ClientSpec.client << " client spec. Consider changing the client spec so that it does. Exiting.");
		return 1;
	}

	int networkThreads = 1;
	std::string networkThreadsStr = Arguments::GetSingleton()->GetNetworkThreads();
	if (!networkThreadsStr.empty())
	{
		networkThreads = std::atoi(networkThreadsStr.c_str());
	}

	int printBatch = 1;
	std::string printBatchStr = Arguments::GetSingleton()->GetPrintBatch();
	if (!printBatchStr.empty())
	{
		printBatch = std::atoi(printBatchStr.c_str());
	}

	int lookAhead = 1;
	std::string lookAheadStr = Arguments::GetSingleton()->GetLookAhead();
	if (!lookAheadStr.empty())
	{
		lookAhead = std::atoi(lookAheadStr.c_str());
	}

	std::string retriesStr = Arguments::GetSingleton()->GetRetries();
	if (!retriesStr.empty())
	{
		P4API::CommandRetries = std::atoi(retriesStr.c_str());
	}

	std::string refreshStr = Arguments::GetSingleton()->GetRefresh();
	if (!refreshStr.empty())
	{
		P4API::CommandRefreshThreshold = std::atoi(refreshStr.c_str());
	}

	bool profiling = false;
#if MTR_ENABLED
	profiling = true;
#endif

	PRINT("Perforce Port: " << P4API::P4PORT);
	PRINT("Perforce User: " << P4API::P4USER);
	PRINT("Perforce Client: " << P4API::P4CLIENT);
	PRINT("Depot Path: " << depotPath);
	PRINT("Network Threads: " << networkThreads);
	PRINT("Print Batch: " << printBatch);
	PRINT("Look Ahead: " << lookAhead);
	PRINT("Max Retries: " << retriesStr);
	PRINT("Max Changes: " << maxChanges);
	PRINT("Refresh Threshold: " << refreshStr);
	PRINT("Fsync Enable: " << fsyncEnable);
	PRINT("Include Binaries: " << includeBinaries);
	PRINT("Profiling: " << profiling);
	PRINT("Profiling Flush Rate: " << flushRate);

	GitAPI git(fsyncEnable);

	if (!git.InitializeRepository(srcPath))
	{
		ERR("Could not initialize Git repository. Exiting.");
		return 1;
	}

	// Setup trace file generation
	mtr_init((srcPath + (srcPath.back() == '/' ? "" : "/") + "trace.json").c_str());
	MTR_META_PROCESS_NAME("p4-fusion");
	MTR_META_THREAD_NAME("Main Thread");
	MTR_META_THREAD_SORT_INDEX(0);

	std::string resumeFromCL;
	if (git.IsHEADExists())
	{
		if (!git.IsRepositoryClonedFrom(depotPath))
		{
			ERR("Git repository at " << srcPath << " was not initially cloned with depotPath = " << depotPath << ". Exiting.");
			return 1;
		}

		resumeFromCL = git.DetectLatestCL();
		WARN("Detected last CL committed as CL " << resumeFromCL);
	}

	PRINT("Requesting changelists to convert from the Perforce server");

	std::vector<ChangeList> changes = p4.Changes(depotPath, resumeFromCL, maxChanges).GetChanges();

	// Return early if we have no work to do
	if (changes.empty())
	{
		SUCCESS("Repository is up to date. Exiting.");
		return 0;
	}

	// The changes are received in chronological order
	SUCCESS("Found " << changes.size() << " uncloned CLs starting from CL " << changes.front().number << " to CL " << changes.back().number);

	PRINT("Creating " << networkThreads << " network threads");
	ThreadPool::GetSingleton()->Initialize(networkThreads);

	int startupDownloadsCount = 0;
	long long lastDownloadCL = -1;

	// Go in the chronological order
	for (size_t i = 0; i < changes.size(); i++)
	{
		if (startupDownloadsCount == lookAhead)
		{
			break;
		}

		// Start running `p4 print` on changed files
		ChangeList& cl = changes.at(i);
		cl.PrepareDownload();
		cl.StartDownload(depotPath, printBatch, includeBinaries);
		startupDownloadsCount++;
		lastDownloadCL = i;
	}

	SUCCESS("Queued first " << startupDownloadsCount << " CLs for downloading");

	int timezoneMinutes = p4.Info().GetServerTimezoneMinutes();

	// Map usernames to emails
	const UsersResult& usersResult = p4.Users();
	const std::unordered_map<std::string, std::string>& users = usersResult.GetUserEmails();

	SUCCESS("Received usernames and emails of Perforce users");

	// Commit procedure start
	Timer commitTimer;

	git.CreateIndex();
	for (size_t i = 0; i < changes.size(); i++)
	{
		// See if the threadpool encountered any exceptions
		try
		{
			ThreadPool::GetSingleton()->RaiseCaughtExceptions();
		}
		catch (const std::exception& e)
		{
			// Threadpool encountered an exception, this is unrecoverable
			ERR("Threadpool encountered an exception: " << e.what());
			ThreadPool::GetSingleton()->ShutDown();
			std::exit(1);
		}

		ChangeList& cl = changes.at(i);

		// Ensure the files are downloaded before committing them to the repository
		cl.WaitForDownload();

		for (auto& file : cl.changedFiles)
		{
			if (file.shouldCommit) // If the file survived the filters while being downloaded
			{
				if (p4.IsDeleted(file.action))
				{
					git.RemoveFileFromIndex(file.depotFile);
				}
				else
				{
					git.AddFileToIndex(file.depotFile, file.contents);
				}

				// No use for keeping the contents in memory once it has been added
				file.Clear();
			}
		}

		std::string email = "deleted@user";
		if (users.find(cl.user) != users.end())
		{
			email = users.at(cl.user);
		}
		std::string commitSHA = git.Commit(depotPath,
		    cl.number,
		    cl.user,
		    email,
		    timezoneMinutes,
		    cl.description,
		    cl.timestamp);

		SUCCESS(
		    "CL " << cl.number << " --> Commit " << commitSHA << " with " << cl.changedFiles.size() << " files (" << i << "/" << changes.size() << "|" << i - lastDownloadCL << "). "
		          << "Elapsed " << commitTimer.GetTimeS() / 60.0f << " mins. " << ((commitTimer.GetTimeS() / 60.0f) / (float)(i + 1)) * (changes.size() - i - 1) << " mins left.");

		// Start downloading the CL chronologically after the last CL that was previously downloaded, if there's still some left
		if (lastDownloadCL + 1 < changes.size())
		{
			lastDownloadCL++;
			ChangeList& downloadCL = changes.at(lastDownloadCL);
			downloadCL.PrepareDownload();
			downloadCL.StartDownload(depotPath, printBatch, includeBinaries);
		}

		// Occasionally flush the profiling data
		if ((i % flushRate) == 0)
		{
			mtr_flush();
		}

		// Deallocate this CL's metadata from memory
		cl.Clear();
	}
	git.CloseIndex();

	SUCCESS("Completed conversion of " << changes.size() << " CLs in " << programTimer.GetTimeS() / 60.0f << " minutes, taking " << commitTimer.GetTimeS() / 60.0f << " to commit CLs");

	ThreadPool::GetSingleton()->ShutDown();

	if (!P4API::ShutdownLibraries())
	{
		return 1;
	}

	mtr_flush();
	mtr_shutdown();

	return 0;
}

void SignalHandler(sig_atomic_t s)
{
	static bool called = false;
	if (called)
	{
		WARN("Already received an interrupt signal, waiting for threads to shutdown.");
		return;
	}
	called = true;

	ERR("Signal Received: " << strsignal(s));

	ThreadPool::GetSingleton()->ShutDown();

	std::exit(1);
}

int main(int argc, char** argv)
{
	signal(SIGINT, SignalHandler);

	try
	{
		Main(argc, argv);
	}
	catch (const std::exception& e)
	{
		ERR("Exception occurred: " << typeid(e).name() << ": " << e.what());
		return 1;
	}

	return 0;
}
