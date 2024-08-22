/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include <atomic>
#include <fstream>
#include <string>
#include <unordered_map>

#include "utils/timer.h"
#include "utils/arguments.h"

#include "thread_pool.h"
#include "p4_api.h"
#include "git_api.h"
#include "branch_set.h"
#include "tracer.h"
#include "labels_conversion.h"

#define P4_FUSION_VERSION "v1.14.1-sg"

void writeString(std::ofstream& outFile, const std::string& str)
{
	size_t length = str.size();
	outFile.write(reinterpret_cast<const char*>(&length), sizeof(length));
	outFile.write(str.data(), length);
}

void writeVectorOfStrings(std::ofstream& outFile, const std::vector<std::string>& vec)
{
	size_t vecSize = vec.size();
	outFile.write(reinterpret_cast<const char*>(&vecSize), sizeof(vecSize));
	for (const auto& str : vec)
	{
		writeString(outFile, str);
	}
}

void writeStructToDisk(std::ofstream& outFile, const LabelResult& labels)
{
	writeString(outFile, labels.label);
	writeString(outFile, labels.revision);
	writeString(outFile, labels.description);
	writeString(outFile, labels.update);
	writeVectorOfStrings(outFile, labels.views);
}

void writeLabelMapToDisk(const std::string& filename, const LabelNameToDetails& labelMap, const std::string& cacheFile)
{
	std::ofstream outFile(filename, std::ios::binary);
	if (!outFile)
	{
		ERR("Error opening " << cacheFile << " file for writing, could not cache labels!");
		return;
	}

	// Write the size of the map
	size_t size = labelMap.size();
	outFile.write(reinterpret_cast<const char*>(&size), sizeof(size));

	// Write each key-value pair
	for (const auto& pair : labelMap)
	{
		writeStructToDisk(outFile, pair.second);
	}

	outFile.close();
}

std::string readString(std::ifstream& inFile)
{
	size_t length;
	inFile.read(reinterpret_cast<char*>(&length), sizeof(length));
	std::string str(length, '\0');
	inFile.read(&str[0], length);
	return str;
}

std::vector<std::string> readVectorOfStrings(std::ifstream& inFile)
{
	size_t vecSize;
	inFile.read(reinterpret_cast<char*>(&vecSize), sizeof(vecSize));
	std::vector<std::string> vec(vecSize);
	for (size_t i = 0; i < vecSize; ++i)
	{
		vec[i] = readString(inFile);
	}
	return vec;
}

LabelResult readStructFromDisk(std::ifstream& inFile)
{
	LabelResult label;
	label.label = readString(inFile);
	label.revision = readString(inFile);
	label.description = readString(inFile);
	label.update = readString(inFile);
	label.views = readVectorOfStrings(inFile);
	return label;
}

LabelNameToDetails readLabelMapFromDisk(const std::string& filename)
{
	LabelNameToDetails labelMap;
	std::ifstream inFile(filename, std::ios::binary);
	if (!inFile)
	{
		ERR("No label cache found at " << filename)
		return labelMap;
	}

	// Read the size of the map
	size_t size;
	inFile.read(reinterpret_cast<char*>(&size), sizeof(size));

	// Read each key-value pair and insert into the map
	for (size_t i = 0; i < size; ++i)
	{
		LabelResult value = readStructFromDisk(inFile);
		labelMap.insert({ value.label, value });
	}

	inFile.close();
	return labelMap;
}

struct compareResponse
{
	std::list<LabelsResult::LabelData> labelsToFetch;
	LabelNameToDetails resultingLabels; // labelMap with the labels that no longer exit removed
};

// Compares the last updated date in the labels list to the updated dates
// in the cache, and returns all labels of which the last updated date is
// different.
compareResponse compareLabelsToCache(const std::list<LabelsResult::LabelData>& labels, LabelNameToDetails& labelMap)
{
	LabelNameToDetails newLabelMap;
	std::list<LabelsResult::LabelData> labelsToFetch;
	for (const auto& label : labels)
	{
		if (labelMap.contains(label.label))
		{
			LabelResult cachedLabel
			    = labelMap.at(label.label);
			if (cachedLabel.update != label.update)
			{
				labelsToFetch.push_back(label);
			}
			else
			{
				newLabelMap.insert({ label.label, cachedLabel });
			}
		}
		else
		{
			labelsToFetch.push_back(label);
		}
	}

	return compareResponse {
		.labelsToFetch = labelsToFetch,
		.resultingLabels = newLabelMap
	};
}

// We need to figure out which labels to delete as well?
// Okay, split it into two functions. Don't be stupid.
// Or make a struct, babyyyyyy

int fetchAndUpdateLabels(P4API& p4, GitAPI& git, const std::string& depotPath, const std::string& cachePath)
{
	// Load labels
	PRINT("Requesting labels from the Perforce server")
	LabelsResult labelsRes = p4.Labels();
	if (labelsRes.HasError())
	{
		ERR("Failed to retrieve labels for mapping: " << labelsRes.PrintError())
		return 1;
	}
	const std::list<LabelsResult::LabelData>& labels = labelsRes.GetLabels();
	SUCCESS("Received " << labels.size() << " labels from the Perforce server")

	LabelNameToDetails cachedLabels;
	if (cachePath.size() > 0)
	{
		PRINT("Reading labels from cache")
		cachedLabels = readLabelMapFromDisk(cachePath);
		SUCCESS("Successfully read " << cachedLabels.size() << " cached labels")
	}

	PRINT("Comparing cached labels with labels from the Perforce server")
	compareResponse compResp = compareLabelsToCache(labels, cachedLabels);

	PRINT("Fetching " << compResp.labelsToFetch.size() << " new label details")
	LabelNameToDetails fetchedLabelMap = getLabelsDetails2(&p4, compResp.labelsToFetch);

	// Join the new map with the old map
	for (const auto& pair : fetchedLabelMap)
	{
		compResp.resultingLabels.insert({ pair.first, pair.second });
	}

	PRINT("Caching updated labels to " << cachePath)
	writeLabelMapToDisk(cachePath, compResp.resultingLabels, cachePath);

	LabelMap revToLabel = getLabelsDetails(&p4, depotPath, compResp.resultingLabels);

	PRINT("Updating tags.")
	git.CreateTagsFromLabels(revToLabel);
	return 0;
}

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

	// Return early if we have no work to do
	if (changes.empty())
	{
		SUCCESS("Repository is up to date.")

		if (!arguments.GetNoConvertLabels())
		{
			return fetchAndUpdateLabels(p4, git, depotPath, arguments.GetLabelCache());
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
		P4API p4labelsClient;
		return fetchAndUpdateLabels(p4labelsClient, git, depotPath, arguments.GetLabelCache());
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
