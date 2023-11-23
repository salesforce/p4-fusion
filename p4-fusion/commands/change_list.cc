/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "change_list.h"

#include <utility>

#include "p4_api.h"
#include "describe_result.h"
#include "filelog_result.h"
#include "print_result.h"
#include "utils/std_helpers.h"
#include "minitrace.h"

ChangeList::ChangeList(const int& clNumber, std::string&& clDescription, std::string&& userID, const int64_t& clTimestamp)
    : number(clNumber)
    , user(std::move(userID))
    , description(std::move(clDescription))
    , timestamp(clTimestamp)
    , waiting(false)
{
}

void flush(P4API& p4, GitAPI& git, const std::vector<std::shared_ptr<FileData>>& printBatchFileData)
{
	MTR_SCOPE("ChangeList", __func__);

	// Only perform the batch processing when there are files to process.
	if (printBatchFileData.empty())
	{
		return;
	}

	std::vector<std::string> fileRevisions;
	fileRevisions.reserve(printBatchFileData.size());
	for (auto& fileData : printBatchFileData)
	{
		std::string fileSpec = fileData->GetDepotFile();
		fileSpec.append("#");
		fileSpec.append(fileData->GetRevision());
		fileRevisions.push_back(fileSpec);
	}

	// Now we write the files that PrintFiles will give us to the git ODB in a
	// streaming fashion.
	// Idx keeps track of the files index in printBatchFileData and is increased
	// every time we are told about a new file.
	// The PrintFiles function takes two callbacks, one for stat, basically "a new
	// file begins here", and then for small chunks of data of that file.
	long idx = -1;
	BlobWriter writer = git.WriteBlob();
	std::function<void()> onNextFile([&idx, &writer, &git, &printBatchFileData]
	    {
			// For the first file, we don't need to run finalize on the previous
			// file so we're done here.
		    if (idx == -1)
		    {
			    idx++;
			    return;
		    }
		    // First, finalize the previous file.
		    printBatchFileData.at(idx)->SetBlobOID(writer.Close());
			// Now step one file further.
		    idx++;
			// And start a write for the next file.
		    writer = git.WriteBlob(); });

	std::function<void(const char*, int)> onWrite([&writer](const char* contents, int length)
	    {
		    // Write a chunk of the data to the currently processed file.
		    writer.Write(contents, length); });

	PrintResult printResp
	    = p4.PrintFiles(fileRevisions, onNextFile, onWrite);
	if (printResp.HasError())
	{
		throw std::runtime_error(printResp.PrintError());
	}
	// If we have seen at least one file, we have to flush the last open file
	// to the ODB still, so let's do that.
	if (idx > -1)
	{
		printBatchFileData.back()->SetBlobOID(writer.Close());
	}
}

void ChangeList::StartDownload(P4API& p4, GitAPI& git, const BranchSet& branchSet, const int& printBatch)
{
	MTR_SCOPE("ChangeList", __func__);

	// First, we need to figure out which files we need to download.
	std::unique_ptr<ChangedFileGroups> changedFileGroups = ChangedFileGroups::Empty();

	if (branchSet.HasMergeableBranch())
	{
		// If we care about branches, we need to run filelog to get where the file came from.
		// Note that the filelog won't include the source changelist, but
		// that doesn't give us too much information; even a full branch
		// copy will have the target files listing the from-file with
		// different changelists than the point-in-time source branch's
		// changelist.
		const FileLogResult& filelog = p4.FileLog(number);
		if (filelog.HasError())
		{
			throw std::runtime_error(filelog.PrintError());
		}
		changedFileGroups = branchSet.ParseAffectedFiles(filelog.GetFileData());
	}
	else
	{
		// If we don't care about branches, then p4->Describe is much faster.
		const DescribeResult& describe = p4.Describe(number);
		if (describe.HasError())
		{
			ERR("Failed to describe changelist: " << describe.PrintError())
			throw std::runtime_error(describe.PrintError());
		}
		changedFileGroups = branchSet.ParseAffectedFiles(describe.GetFileData());
	}

	// Now, we download the files in batches.

	std::vector<std::shared_ptr<FileData>> printBatchFileData;
	// Only perform the group inspection if there are files.
	if (changedFileGroups->totalFileCount > 0)
	{
		for (auto& branchedFileGroup : changedFileGroups->branchedFileGroups)
		{
			// Note: the files at this point have already been filtered.
			for (auto& fileData : branchedFileGroup.files)
			{
				if (fileData.IsDownloadNeeded())
				{
					fileData.SetPendingDownload();
					printBatchFileData.push_back(std::make_shared<FileData>(fileData));

					// Clear the batches if it fits
					if (printBatchFileData.size() >= printBatch)
					{
						flush(p4, git, printBatchFileData);

						// We let go of the refs held by us and create new ones to queue the next batch
						printBatchFileData.clear();
						// Now only the thread job has access to the older batch
					}
				}
			}
		}
	}

	// Flush any remaining files that were smaller in number than the total batch size.
	// Additionally, signal the batch processing end.
	flush(p4, git, printBatchFileData);
	{
		std::unique_lock<std::mutex> lock(*commitMutex);
		*downloadJobsCompleted = true;
		if (waiting)
		{
			commitCV->notify_all();
		}
	}
}

void ChangeList::WaitForDownload()
{
	MTR_SCOPE("ChangeList", __func__);

	std::unique_lock<std::mutex> lock(*commitMutex);
	waiting = true;
	commitCV->wait(lock, [this]()
	    { return downloadJobsCompleted->load(); });
}
