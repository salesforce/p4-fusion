/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include <thread>
#include <atomic>
#include <unordered_map>
#include <typeinfo>

#include "common.h"

#include "utils/timer.h"
#include "utils/arguments.h"

#include "thread_pool.h"
#include "p4_api.h"
#include "git_api.h"
#include "branch_set.h"
#include "tracer.h"
#include "git2/commit.h"
#include "labels_conversion.h"

#define P4_FUSION_VERSION "v1.14.0-sg"

int Main(int argc, char** argv)
{
	// Initialize a program timer to track total execution time.
	Timer programTimer;

	PRINT("p4-fusion " << P4_FUSION_VERSION << " running from " << argv[0])

	Arguments arguments(argc, argv);
	if (!arguments.IsValid())
	{
		PRINT("Usage:" + arguments.Help())
		return 1;
	}
	if (arguments.GetNoColor())
	{
		Log::DisableColoredOutput();
	}

	arguments.Print();

	// Initialize a minitrace based tracer.
	Tracer tracer(arguments.GetSourcePath(), arguments.GetFlushRate());

	// Initialize the P4Libraries API.
	// It will be uninitialized once this function returns.
	P4LibrariesRAII lib;

	// Initialize libgit2.
	// It will be uninitialized once this function returns.
	Libgit2RAII libgit2(arguments.GetFsyncEnable());

	// Setup global config for P4API.
	P4API::P4PORT = arguments.GetPort();
	P4API::P4USER = arguments.GetUsername();
	P4API::CommandRetries = arguments.GetRetries();
	P4API::CommandRefreshThreshold = arguments.GetRefresh();
	P4API::P4CLIENT = arguments.GetClient();

	// Create the p4 API for the main thread.
	P4API p4;

	{
		TestResult serviceConnectionResult = p4.TestConnection(5);
		if (serviceConnectionResult.HasError())
		{
			ERR("Error occurred while connecting to " << P4API::P4PORT << ": " << serviceConnectionResult.PrintError())
			return 1;
		}

		SUCCESS("Perforce server is available")
	}

	{
		ClientResult clientRes = p4.Client();
		if (clientRes.HasError())
		{
			ERR("Error occurred while fetching client spec: " + clientRes.PrintError())
			return 1;
		}
		P4API::ClientSpec = clientRes.GetClientSpec();

		if (P4API::ClientSpec.mapping.empty())
		{
			ERR("Received a client spec with no mappings. Did you use the correct corresponding P4PORT for the " + P4API::ClientSpec.client + " client spec?")
			return 1;
		}

		PRINT("Updated client workspace view " << P4API::ClientSpec.client << " with " << P4API::ClientSpec.mapping.size() << " mappings")
	}

	auto depotPath = arguments.GetDepotPath();
	auto srcPath = arguments.GetSourcePath();
	auto printBatch = arguments.GetPrintBatch();

	if (!P4API::IsDepotPathValid(depotPath))
	{
		ERR("Depot path should begin with \"//\" and end with \"/...\". Please pass in the proper depot path and try again.")
		return 1;
	}

	if (!p4.IsDepotPathUnderClientSpec(depotPath))
	{
		ERR("The depot path specified is not under the " << P4API::ClientSpec.client << " client spec. Consider changing the client spec so that it does. Exiting.")
		return 1;
	}

	int timezoneMinutes;
	{
		InfoResult p4infoRes = p4.Info();
		if (p4infoRes.HasError())
		{
			ERR("Failed to fetch Perforce server timezone: " << p4infoRes.PrintError())
			return 1;
		}
		timezoneMinutes = p4infoRes.GetServerTimezoneMinutes();
		SUCCESS("Perforce server timezone is " << timezoneMinutes << " minutes")
	}

	GitAPI git(srcPath, timezoneMinutes);

	// This throws on error. It should be called before the ThreadPool is created.
	git.InitializeRepository(arguments.GetNoBaseCommit());

	std::string resumeFromCL;
	if (git.IsHEADExists())
	{
		if (!git.IsRepositoryClonedFrom(depotPath))
		{
			ERR("Git repository at " << srcPath << " was not initially cloned with depotPath = " << depotPath << ". Exiting.")
			return 1;
		}

		resumeFromCL = git.DetectLatestCL();
		SUCCESS("Detected last CL committed as CL " << resumeFromCL)
	}

	// Request changelists.
	PRINT("Requesting changelists to convert from the Perforce server")
	std::deque<ChangeList> changes;
	{
		ChangesResult changesRes = p4.Changes(depotPath, resumeFromCL, arguments.GetMaxChanges());
		if (changesRes.HasError())
		{
			ERR("Failed to list changes: " << changesRes.PrintError())
			return 1;
		}
		changes = std::move(changesRes.GetChanges());
	}

	// Load labels
	PRINT("Requesting labels from the Perforce server")
	LabelsResult labelsRes = p4.Labels();
	if (labelsRes.HasError())
	{
		ERR("Failed to retrieve labels for mapping: " << labelsRes.PrintError())
		return 1;
	}
	const std::list<std::string>& labels = labelsRes.GetLabels();
	SUCCESS("Received " << labels.size() << " labels from the Perforce server")

	LabelMap revToLabel = getLabelsDetails(&p4, depotPath, labels);

	// Return early if we have no work to do
	if (changes.empty())
	{
		SUCCESS("Repository is up to date.")

		if (!arguments.GetNoConvertLabels())
		{
			SUCCESS("Updating tags.")
			git.CreateTagsFromLabels(revToLabel);
		}

		return 0;
	}
	SUCCESS("Found " << changes.size() << " uncloned CLs starting from CL " << changes.front().number << " to CL " << changes.back().number)

	BranchSet branchSet(P4API::ClientSpec.mapping, depotPath, arguments.GetBranches(), arguments.GetIncludeBinaries());
	if (branchSet.Count() > 0)
	{
		PRINT("Inspecting " << branchSet.Count() << " branches")
	}

	// Load mapping data from usernames to emails.
	PRINT("Requesting userbase details from the Perforce server")
	UsersResult usersRes = p4.Users();
	if (usersRes.HasError())
	{
		ERR("Failed to retrieve user details for mapping: " << usersRes.PrintError())
		return 1;
	}
	const std::unordered_map<UsersResult::UserID, UsersResult::UserData>& users = usersRes.GetUserEmails();
	SUCCESS("Received " << users.size() << " userbase details from the Perforce server")

	// Create the thread pool
	int networkThreads = arguments.GetNetworkThreads();
	if (networkThreads > changes.size())
	{
		networkThreads = int(changes.size());
	}
	PRINT("Creating " << networkThreads << " network threads")
	ThreadPool pool(networkThreads, srcPath, timezoneMinutes);
	SUCCESS("Created " << pool.GetThreadCount() << " threads in thread pool")

	// Go in the chronological order.
	std::atomic<int> nextToEnqueue;
	std::atomic<int> downloaded;
	downloaded.store(0);
	size_t startupDownloadsCount = arguments.GetLookAhead();
	if (startupDownloadsCount > changes.size())
	{
		startupDownloadsCount = changes.size();
	}
	// First, we enqueue the initial set of changelists for download, at most
	// lookAhead jobs.
	for (size_t currentCL = 0; currentCL < startupDownloadsCount; currentCL++)
	{
		ChangeList& cl = changes.at(currentCL);

		nextToEnqueue++;

		pool.AddJob([&downloaded, &cl, &branchSet, printBatch](P4API& p4, GitAPI& git)
		    {
			cl.StartDownload(p4, git, branchSet, printBatch);
			// Mark download as done.
			downloaded++; });
	}

	SUCCESS("Queued first " << startupDownloadsCount << " CLs up until CL " << changes.at(startupDownloadsCount - 1).number << " for downloading")

	// Commit procedure start
	Timer commitTimer;

	auto totalChanges = changes.size();
	auto noMerge = arguments.GetNoMerge();
	int i(0);
	while (!changes.empty())
	{
		// Ensure the files are downloaded before committing them to the repository
		// First, wait until downloaded so the changelist is no longer referenced
		// in worker threads.
		changes.front().WaitForDownload();

		// Now move the changelist and pop it off the queue.
		// Once this iteration is over, it will be destructed
		// and memory is freed.
		ChangeList cl = std::move(changes.front());
		changes.pop_front();

		std::string fullName(cl.user);
		std::string email("deleted@user");
		if (users.find(cl.user) != users.end())
		{
			fullName = users.at(cl.user).fullName;
			email = users.at(cl.user).email;
		}

		for (auto& branchGroup : cl.changedFileGroups->branchedFileGroups)
		{
			std::string mergeFrom;
			if (branchGroup.hasSource && !noMerge)
			{
				// Only perform merging if the branch group explicitly declares that the change
				// has a source, and if the user wants merging.
				mergeFrom = branchGroup.sourceBranch;
			}

			const std::string& commitSHA = git.WriteChangelistBranch(
			    depotPath,
			    cl,
			    branchGroup.files,
			    branchGroup.targetBranch,
			    fullName,
			    email,
			    mergeFrom);

#ifdef PRINT_TEST_OUTPUT
			// For scripting/testing purposes...
			PRINT("COMMIT:" << commitSHA << ":" << cl.number << ":" << branchGroup.targetBranch << ":")
#endif
			if (branchSet.Count() > 0)
			{
				SUCCESS(
				    "CL " << cl.number << " --> Commit " << commitSHA
				          << " with " << branchGroup.files.size() << " files"
				          << (branchGroup.targetBranch.empty()
				                     ? ""
				                     : (" to branch " + branchGroup.targetBranch))
				          << (branchGroup.sourceBranch.empty()
				                     ? ""
				                     : (" from branch " + branchGroup.sourceBranch))
				          << ".")
			}
		}
		SUCCESS(
		    "CL " << cl.number << " with "
		          << cl.changedFileGroups->totalFileCount << " files (" << i + 1 << "/" << totalChanges
		          << "|" << downloaded
		          << "). Elapsed " << commitTimer.GetTimeS() / 60.0f << " mins. "
		          << ((commitTimer.GetTimeS() / 60.0f) / (float)(i + 1)) * (totalChanges - i - 1) << " mins left.")

		i++;

		// Once a cl has been downloaded, we check if we can enqueue a new job
		// for background downloading.
		if (changes.size() > (nextToEnqueue - i))
		{
			ChangeList& downloadCL = changes.at(nextToEnqueue - i);
			nextToEnqueue++;
			pool.AddJob([&downloaded, &downloadCL, &branchSet, printBatch](P4API& p4, GitAPI& git)
			    {
				downloadCL.StartDownload(p4, git, branchSet, printBatch);
				// Mark download as done.
				downloaded++; });
		}
	}

	SUCCESS("Completed conversion of " << totalChanges << " CLs in " << programTimer.GetTimeS() / 60.0f << " minutes, taking " << commitTimer.GetTimeS() / 60.0f << " to commit CLs")

	if (!arguments.GetNoConvertLabels())
	{
		SUCCESS("Updating tags.")
		git.CreateTagsFromLabels(revToLabel);
	}

	return 0;
}

int main(int argc, char** argv)
{
	int exitCode;

	try
	{
		exitCode = Main(argc, argv);
	}
	catch (const std::exception& e)
	{
		ERR("Exception occurred: " << e.what())
		return 1;
	}

	return exitCode;
}
