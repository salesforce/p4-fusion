/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "change_list.h"

#include "p4_api.h"
#include "describe_result.h"
#include "filelog_result.h"
#include "print_result.h"
#include "utils/std_helpers.h"
#include "minitrace.h"

#include "thread_pool.h"

ChangeList::ChangeList(const std::string& clNumber, const std::string& clDescription, const std::string& userID, const int64_t& clTimestamp)
    : number(clNumber)
    , user(userID)
    , description(clDescription)
    , timestamp(clTimestamp)
    , changedFileGroups(ChangedFileGroups::Empty())
{
}

void ChangeList::PrepareDownload(P4API* p4, const BranchSet& branchSet)
{
	MTR_SCOPE("ChangeList", __func__);

	if (branchSet.HasMergeableBranch())
	{
		// If we care about branches, we need to run filelog to get where the file came from.
		// Note that the filelog won't include the source changelist, but
		// that doesn't give us too much information; even a full branch
		// copy will have the target files listing the from-file with
		// different changelists than the point-in-time source branch's
		// changelist.
		const FileLogResult& filelog = p4->FileLog(number);
		if (filelog.HasError())
		{
			throw filelog.PrintError();
		}
		changedFileGroups = branchSet.ParseAffectedFiles(filelog.GetFileData());
	}
	else
	{
		// If we don't care about branches, then p4->Describe is much faster.
		const DescribeResult& describe = p4->Describe(number);
		if (describe.HasError())
		{
			ERR("Failed to describe changelist: " << describe.PrintError());
			throw describe.PrintError();
		}
		changedFileGroups = branchSet.ParseAffectedFiles(describe.GetFileData());
	}

	*filesDownloaded = 0;
	{
		std::unique_lock<std::mutex> lock(*downloadPreparedMutex);
		*downloadPrepared = true;
		downloadPreparedCV->notify_all();
	}
}

void ChangeList::StartDownload(P4API* p4, const BranchSet& branchSet, const int& printBatch)
{
	MTR_SCOPE("ChangeList", __func__);

	// wait for prepare to be finished.

	{
		std::unique_lock<std::mutex> lock(*downloadPreparedMutex);
		downloadPreparedCV->wait(lock, [this]()
		    { return downloadPrepared->load(); });
	}

	std::shared_ptr<std::vector<std::string>> printBatchFiles = std::make_shared<std::vector<std::string>>();
	std::shared_ptr<std::vector<FileData*>> printBatchFileData = std::make_shared<std::vector<FileData*>>();
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
					printBatchFiles->push_back(fileData.GetDepotFile() + "#" + fileData.GetRevision());
					printBatchFileData->push_back(&fileData);

					// Clear the batches if it fits
					if (printBatchFiles->size() == printBatch)
					{
						Flush(p4, printBatchFiles, printBatchFileData);

						// We let go of the refs held by us and create new ones to queue the next batch
						printBatchFiles = std::make_shared<std::vector<std::string>>();
						printBatchFileData = std::make_shared<std::vector<FileData*>>();
						// Now only the thread job has access to the older batch
					}
				}
			}
		}
	}

	// Flush any remaining files that were smaller in number than the total batch size.
	// Additionally, signal the batch processing end.
	Flush(p4, printBatchFiles, printBatchFileData);
	*downloadJobsCompleted = true;
	commitCV->notify_all();
}

void ChangeList::Flush(P4API* p4, std::shared_ptr<std::vector<std::string>> printBatchFiles, std::shared_ptr<std::vector<FileData*>> printBatchFileData)
{
	// Only perform the batch processing when there are files to process.
	if (!printBatchFileData->empty())
	{
		const PrintResult& printData = p4->PrintFiles(*printBatchFiles);
		if (printData.HasError())
		{
			throw printData.PrintError();
		}

		for (int i = 0; i < printBatchFiles->size(); i++)
		{
			printBatchFileData->at(i)->MoveContentsOnceFrom(printData.GetPrintData().at(i).contents);
		}

		(*filesDownloaded) += printBatchFiles->size();
	}

	// Ensure the notify_all is called.
	commitCV->notify_all();
}

void ChangeList::WaitForDownload()
{
	std::unique_lock<std::mutex> lock(*commitMutex);
	commitCV->wait(lock, [this]()
	    { return downloadJobsCompleted->load(); });
}

void ChangeList::Clear()
{
	number.clear();
	user.clear();
	description.clear();
	changedFileGroups->Clear();

	filesDownloaded.reset();
	commitMutex.reset();
	commitCV.reset();
}
