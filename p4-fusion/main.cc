/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include <thread>
#include <map>
#include <atomic>
#include <sstream>
#include <typeinfo>
#include <csignal>
#include <cerrno>

#include "common.h"

#include "utils/timer.h"
#include "utils/arguments.h"

#include "thread_pool.h"
#include "p4_api.h"
#include "git_api.h"
#include "branch_set.h"

#include "p4/p4libs.h"
#include "minitrace.h"

#define P4_FUSION_VERSION "v1.13.1-sg"

void shutdown(ThreadPool& pool);

int Main(int argc, char** argv)
{
	Timer programTimer;

	PRINT("p4-fusion " P4_FUSION_VERSION);

	Arguments arguments(argc, argv);
	if (!arguments.IsValid())
	{
		PRINT("Usage:" + arguments.Help());
		return 1;
	}

	const bool noColor = arguments.GetNoColor();
	if (noColor)
	{
		Log::DisableColoredOutput();
	}

	const bool noMerge = arguments.GetNoMerge();
	const std::string depotPath = arguments.GetDepotPath();
	const std::string srcPath = arguments.GetSourcePath();
	const bool fsyncEnable = arguments.GetFsyncEnable();
	const bool includeBinaries = arguments.GetIncludeBinaries();
	const int maxChanges = arguments.GetMaxChanges();
	const int flushRate = arguments.GetFlushRate();
	const std::vector<std::string> branchNames = arguments.GetBranches();

	PRINT("Running p4-fusion from: " << argv[0]);

	if (!P4API::InitializeLibraries())
	{
		return 1;
	}

	// Block signals from being handled by the main thread, and all future threads.
	//
	// - SIGINT, SIGTERM, SIGHUP: thread pool will be shutdown and std::exit will be called
	// - SIGUSR1: only sent by main() to tell the signal handler thread to exit
	sigset_t blockedSignals;
	sigemptyset(&blockedSignals);
	for (auto sig : { SIGINT, SIGTERM, SIGHUP, SIGUSR1 })
	{
		sigaddset(&blockedSignals, sig);
	}

	int rc = pthread_sigmask(SIG_BLOCK, &blockedSignals, nullptr);
	if (rc != 0)
	{
		ERR("(signal handler) failed to block signals: (" << errno << ") " << strerror(errno));
		return 1;
	}

	// Create the thread pool
	int networkThreads = arguments.GetNetworkThreads();
	PRINT("Creating " << networkThreads << " network threads");
	ThreadPool pool(networkThreads);
	SUCCESS("Created " << pool.GetThreadCount() << " threads in thread pool");

	sigset_t signalsToWaitOn = blockedSignals;
	// Spawn a thread to handle signals.
	// The thread will block and wait for signals to arrive and then shutdown the thread pool, unless it receives SIGUSR1,
	// in which case it will just exit (since main() is handling the shutdown).
	//
	// Using a separate thread for purely signal handling allows us to use non-reentrant functions
	// (such as std::cout, condition variables, etc.) in the signal handler.
	auto signalHandlingThread = std::thread(
	    [signalsToWaitOn, &pool]()
	    {
		    // Wait for signals to arrive.
		    int sig;
		    int rc = sigwait(&signalsToWaitOn, &sig);
		    if (rc != 0)
		    {
			    ERR("(signal handler) failed to wait for signals: (" << errno << ") " << strerror(errno));
			    shutdown(pool);
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
			    ERR("(signal handler): WARNING: received signal (" << sig << ") \"" << strsignal(sig) << "\" that is not blocked, this should not happen and indicates a logic error in the signal handler.");
		    }

		    ERR("(signal handler) received signal (" << sig << ") \"" << strsignal(sig) << "\", shutting down");
		    shutdown(pool);
		    std::exit(sig);
	    });

	P4API::P4PORT = arguments.GetPort();
	P4API::P4USER = arguments.GetUsername();

	TestResult serviceConnectionResult = P4API().TestConnection(5);
	if (serviceConnectionResult.HasError())
	{
		ERR("Error occurred while connecting to " << P4API::P4PORT << ": " << serviceConnectionResult.PrintError());
		return 1;
	}

	SUCCESS("Perforce server is available");

	P4API::P4CLIENT = arguments.GetClient();
	ClientResult clientRes = P4API().Client();
	if (clientRes.HasError())
	{
		ERR("Error occurred while fetching client spec: " + clientRes.PrintError());
		return 1;
	}
	P4API::ClientSpec = clientRes.GetClientSpec();

	if (P4API::ClientSpec.mapping.empty())
	{
		ERR("Received a client spec with no mappings. Did you use the correct corresponding P4PORT for the " + P4API::ClientSpec.client + " client spec?");
		return 1;
	}

	PRINT("Updated client workspace view " << P4API::ClientSpec.client << " with " << P4API::ClientSpec.mapping.size() << " mappings");

	P4API p4;

	InfoResult p4infoRes = p4.Info();
	if (p4infoRes.HasError())
	{
		ERR("Failed to fetch Perforce server timezone: " << p4infoRes.PrintError());
		return 1;
	}
	int timezoneMinutes = p4infoRes.GetServerTimezoneMinutes();
	SUCCESS("Perforce server timezone is " << timezoneMinutes << " minutes");

	if (!p4.IsDepotPathValid(depotPath))
	{
		ERR("Depot path should begin with \"//\" and end with \"/...\". Please pass in the proper depot path and try again.");
		return 1;
	}

	if (!p4.IsDepotPathUnderClientSpec(depotPath))
	{
		ERR("The depot path specified is not under the " << P4API::ClientSpec.client << " client spec. Consider changing the client spec so that it does. Exiting.");
		return 1;
	}

	int printBatch = arguments.GetPrintBatch();
	int lookAhead = arguments.GetLookAhead();
	P4API::CommandRetries = arguments.GetRetries();
	P4API::CommandRefreshThreshold = arguments.GetRefresh();
	bool profiling = false;
#if MTR_ENABLED
	profiling = true;
#endif
	const std::string tracePath = (srcPath + (srcPath.back() == '/' ? "" : "/") + "trace.json");

	BranchSet branchSet(P4API::ClientSpec.mapping, depotPath, branchNames, includeBinaries);

	PRINT("Perforce Port: " << P4API::P4PORT);
	PRINT("Perforce User: " << P4API::P4USER);
	PRINT("Perforce Client: " << P4API::P4CLIENT);
	PRINT("Depot Path: " << depotPath);
	PRINT("Network Threads: " << networkThreads);
	PRINT("Print Batch: " << printBatch);
	PRINT("Look Ahead: " << lookAhead);
	PRINT("Max Retries: " << P4API::CommandRetries);
	PRINT("Max Changes: " << maxChanges);
	PRINT("Refresh Threshold: " << P4API::CommandRefreshThreshold);
	PRINT("Fsync Enable: " << fsyncEnable);
	PRINT("Include Binaries: " << includeBinaries);
	PRINT("Profiling: " << profiling << " (" << tracePath << ")");
	PRINT("Profiling Flush Rate: " << flushRate);
	PRINT("No Colored Output: " << noColor);
	PRINT("Inspecting " << branchSet.Count() << " branches");

	GitAPI git(fsyncEnable, timezoneMinutes);

	if (!git.InitializeRepository(srcPath, arguments.GetNoBaseCommit()))
	{
		ERR("Could not initialize Git repository. Exiting.");
		return 1;
	}

	// Setup trace file generation. This HAS to happen after initializing the
	// repository, only then the tracePath will be ensured to exist.
	mtr_init(tracePath.c_str());
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
		SUCCESS("Detected last CL committed as CL " << resumeFromCL);
	}

	// Load mapping data from usernames to emails.
	PRINT("Requesting userbase details from the Perforce server");
	UsersResult usersRes = p4.Users();
	if (usersRes.HasError())
	{
		ERR("Failed to retrieve user details for mapping: " << usersRes.PrintError());
		return 1;
	}
	const std::unordered_map<UsersResult::UserID, UsersResult::UserData> users = std::move(usersRes.GetUserEmails());
	SUCCESS("Received " << users.size() << " userbase details from the Perforce server");

	// Request changelists.
	PRINT("Requesting changelists to convert from the Perforce server");
	ChangesResult changesRes = p4.Changes(depotPath, resumeFromCL, maxChanges);
	if (changesRes.HasError())
	{
		ERR("Failed to list changes: " << changesRes.PrintError());
		return 1;
	}
	std::vector<ChangeList> changes = std::move(changesRes.GetChanges());
	// Return early if we have no work to do
	if (changes.empty())
	{
		SUCCESS("Repository is up to date. Exiting.");
		return 0;
	}
	SUCCESS("Found " << changes.size() << " uncloned CLs starting from CL " << changes.front().number << " to CL " << changes.back().number);

	auto exceptionHandlingThread = std::thread([&pool]()
	    {
			// See if the threadpool encountered any exceptions.
			try
			{
				pool.RaiseCaughtExceptions();
			}
			catch (const std::exception& e)
			{
				// This is unrecoverable
				ERR("Threadpool encountered an exception: " << e.what());
				pool.ShutDown();
				std::exit(1);
			} });

	// Go in the chronological order.
	std::atomic<int> downloaded;
	downloaded.store(0);
	int startupDownloadsCount = lookAhead;
	if (lookAhead > changes.size())
	{
		startupDownloadsCount = changes.size();
	}
	// First, we enqueue the initial set of changelists for download, at most
	// lookAhead jobs.
	for (size_t currentCL = 0; currentCL < startupDownloadsCount; currentCL++)
	{
		ChangeList& cl = changes.at(currentCL);

		pool.AddJob([&downloaded, &cl, &branchSet, printBatch](P4API* p4)
		    { cl.PrepareDownload(p4, branchSet); });
	}
	for (size_t currentCL = 0; currentCL < startupDownloadsCount; currentCL++)
	{
		ChangeList& cl = changes.at(currentCL);

		pool.AddJob([&downloaded, &cl, &branchSet, printBatch](P4API* p4)
		    {
			cl.StartDownload(p4, branchSet, printBatch);
			// Mark download as done.
			downloaded++; });
	}

	SUCCESS("Queued first " << startupDownloadsCount << " CLs up until CL " << changes.at(startupDownloadsCount - 1).number << " for downloading");

	// Commit procedure start
	Timer commitTimer;

	for (size_t i = 0; i < changes.size(); i++)
	{
		ChangeList& cl = changes.at(i);

		// Ensure the files are downloaded before committing them to the repository
		PRINT("Waiting for download of CL " << cl.number << " to complete");
		cl.WaitForDownload();

		std::string fullName = cl.user;
		std::string email = "deleted@user";
		if (users.find(cl.user) != users.end())
		{
			fullName = users.at(cl.user).fullName;
			email = users.at(cl.user).email;
		}

		for (auto& branchGroup : cl.changedFileGroups->branchedFileGroups)
		{
			std::string mergeFrom = "";
			if (branchGroup.hasSource && !noMerge)
			{
				// Only perform merging if the branch group explicitly declares that the change
				// has a source, and if the user wants merging.
				mergeFrom = branchGroup.sourceBranch;
			}

			const std::string commitSHA = git.WriteChangelistBranch(
			    depotPath,
			    cl,
			    branchGroup.files,
			    branchGroup.targetBranch,
			    fullName,
			    email,
			    mergeFrom);

			// For scripting/testing purposes...
			PRINT("COMMIT:" << commitSHA << ":" << cl.number << ":" << branchGroup.targetBranch << ":");
			SUCCESS(
			    "CL " << cl.number << " --> Commit " << commitSHA
			          << " with " << branchGroup.files.size() << " files"
			          << (branchGroup.targetBranch.empty()
			                     ? ""
			                     : (" to branch " + branchGroup.targetBranch))
			          << (branchGroup.sourceBranch.empty()
			                     ? ""
			                     : (" from branch " + branchGroup.sourceBranch))
			          << ".");
		}
		SUCCESS(
		    "CL " << cl.number << " with "
		          << cl.changedFileGroups->totalFileCount << " files (" << i + 1 << "/" << changes.size()
		          << "|" << downloaded
		          << "). Elapsed " << commitTimer.GetTimeS() / 60.0f << " mins. "
		          << ((commitTimer.GetTimeS() / 60.0f) / (float)(i + 1)) * (changes.size() - i - 1) << " mins left.");
		// Clear out finished changelist.
		cl.Clear();

		// Once a cl has been downloaded, we check if we can enqueue a new job
		// right away.
		size_t next = startupDownloadsCount + i;
		if (next < changes.size())
		{
			ChangeList& downloadCL = changes.at(next);
			pool.AddJob([&downloaded, &downloadCL, &branchSet, printBatch](P4API* p4)
			    {
				downloadCL.PrepareDownload(p4, branchSet);
				downloadCL.StartDownload(p4, branchSet, printBatch);
				// Mark download as done.
				downloaded++; });
		}

		// Occasionally flush the profiling data
		if ((i % flushRate) == 0)
		{
			mtr_flush();
		}
	}

	SUCCESS("Completed conversion of " << changes.size() << " CLs in " << programTimer.GetTimeS() / 60.0f << " minutes, taking " << commitTimer.GetTimeS() / 60.0f << " to commit CLs");

	shutdown(pool); // Shut down thread pool, P4API, and tracing.

	rc = pthread_kill(signalHandlingThread.native_handle(), SIGUSR1); // Tell the signal handler thread to exit.
	if (rc != 0)
	{
		ERR("(signal handler) failed to shut down signal handling thread: (" << errno << ") " << strerror(errno));
		return errno;
	}

	signalHandlingThread.join(); // Wait for the signal handler thread to exit.
	exceptionHandlingThread.join(); // Wait for the exception handling thread to exit.

	return 0;
}

void shutdown(ThreadPool& pool)
{
	pool.ShutDown();

	P4API::ShutdownLibraries();

	mtr_flush();
	mtr_shutdown();
}

int main(int argc, char** argv)
{
	int exitCode = 0;

	try
	{
		exitCode = Main(argc, argv);
	}
	catch (const std::exception& e)
	{
		ERR("Exception occurred: " << typeid(e).name() << ": " << e.what());
		return 1;
	}

	return exitCode;
}
