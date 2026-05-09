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

#include "thread_pool.h"

ChangeList::ChangeList(const std::string& clNumber, const std::string& clDescription, const std::string& userID, const int64_t& clTimestamp)
    : number(clNumber)
    , user(userID)
    , description(clDescription)
    , timestamp(clTimestamp)
    , changedFileGroups(ChangedFileGroups::Empty())
    , stateCV(new std::condition_variable())
    , stateMutex(new std::mutex())
    , filesDownloaded(-1)
    , state(Initialized)
{
}

void ChangeList::PrepareDownload(const BranchSet& branchSet)
{
	ChangeList& cl = *this;

	ThreadPool::GetSingleton()->AddJob([&cl, &branchSet](P4API* p4)
	    {
		    std::vector<FileData> changedFiles;
		    if (branchSet.HasMergeableBranch())
		    {
			    // If we care about branches, we need to run filelog to get where the file came from.
			    // Note that the filelog won't include the source changelist, but
			    // that doesn't give us too much information; even a full branch
			    // copy will have the target files listing the from-file with
			    // different changelists than the point-in-time source branch's
			    // changelist.
			    std::unique_ptr<FileLogResult> filelog = p4->FileLog(cl.number);
			    cl.changedFileGroups = branchSet.ParseAffectedFiles(filelog->GetFileData());
		    }
		    else
		    {
			    // If we don't care about branches, then p4->Describe is much faster.
			    std::unique_ptr<DescribeResult> describe = p4->Describe(cl.number);
			    cl.changedFileGroups = branchSet.ParseAffectedFiles(describe->GetFileData());
		    }

		    std::unique_lock<std::mutex> lock(*cl.stateMutex);
		    cl.state = Described;
		    cl.stateCV->notify_all();
	    });
}

void ChangeList::StartDownload(const int& printBatch)
{
	ChangeList& cl = *this;

	ThreadPool::GetSingleton()->AddJob([&cl, printBatch](P4API* p4)
	    {
		    // Wait for describe to finish, if it is still running
		    {
			    std::unique_lock<std::mutex> lock(*cl.stateMutex);
			    cl.stateCV->wait(lock, [&cl]()
			        { return cl.state == Described; });
		    }

		    cl.filesDownloaded = 0;

		    std::shared_ptr<std::vector<std::string>> printBatchFiles = std::make_shared<std::vector<std::string>>();
		    std::shared_ptr<std::vector<FileData*>> printBatchFileData = std::make_shared<std::vector<FileData*>>();
		    // Only perform the group inspection if there are files.
		    if (cl.changedFileGroups->totalFileCount > 0)
		    {
			    for (auto& branchedFileGroup : cl.changedFileGroups->branchedFileGroups)
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
							    cl.Flush(printBatchFiles, printBatchFileData);

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
		    cl.Flush(printBatchFiles, printBatchFileData);
	    });
}

void ChangeList::Flush(std::shared_ptr<std::vector<std::string>> printBatchFiles, std::shared_ptr<std::vector<FileData*>> printBatchFileData)
{
	// Share ownership of this batch with the thread job
	ThreadPool::GetSingleton()->AddJob([this, printBatchFiles, printBatchFileData](P4API* p4)
	    {
		    // Only perform the batch processing when there are files to process.
		    if (!printBatchFileData->empty())
		    {
			    std::unique_ptr<PrintResult> printData = p4->PrintFiles(*printBatchFiles);

			    for (int i = 0; i < printBatchFiles->size(); i++)
			    {
				    printBatchFileData->at(i)->MoveContentsOnceFrom(printData->GetPrintData().at(i).contents);
			    }
		    }

		    std::lock_guard<std::mutex> lock(*stateMutex);
		    filesDownloaded += printBatchFiles->size();
		    if (filesDownloaded == changedFileGroups->totalFileCount)
		    {
			    state = Downloaded;
			    stateCV->notify_all();
		    }
	    });
}

void ChangeList::WaitForDownload()
{
	std::unique_lock<std::mutex> lock(*stateMutex);
	stateCV->wait(lock, [this]()
	    { return state == Downloaded; });
}

void ChangeList::Clear()
{
	number.clear();
	user.clear();
	description.clear();
	changedFileGroups->Clear();

	stateCV.reset();
	stateMutex.reset();
	filesDownloaded = -1;
	state = Freed;
}
