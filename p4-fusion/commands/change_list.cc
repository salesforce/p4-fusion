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
#include "fstat_change_result.h"
#include "print_result.h"
#include "utils/std_helpers.h"

#include "thread_pool.h"

ChangeList::ChangeList(const std::string& clNumber, const std::string& clDescription, const std::string& userID, const int64_t& clTimestamp)
    : number(clNumber)
    , user(userID)
    , description(clDescription)
    , timestamp(clTimestamp)
{
}

void ChangeList::PrepareDownload(const bool includeBranchSource)
{
	ChangeList& cl = *this;

	// While not good isolation, for performance, sticking the
	// branch split into this thread would help.  It's also where
	// the has-branch required check is stored.

	// Cut and paste fun.
	if (includeBranchSource)
	{
		// If we don't care about branches, then p4->Describe is much faster.
		ThreadPool::GetSingleton()->AddJob([&cl](P4API* p4)
			{
				const DescribeResult& describe = p4->Describe(cl.number);
				cl.changedFiles = std::move(describe.GetFileData());

				{
					std::unique_lock<std::mutex> lock((*(cl.canDownloadMutex)));
					*cl.canDownload = true;
				}
				cl.canDownloadCV->notify_all();
			});
	}
	else
	{
		// Otherwise, we need to run filelog to get where the file came from.
		// Note that the filelog won't include the source changelist, but
		// that doesn't give us too much information; even a full branch
		// copy will have the target files listing the from-file with
		// different changelists than the point-in-time source branch's
		// changelist.
		ThreadPool::GetSingleton()->AddJob([&cl](P4API* p4)
			{
				const FileLogResult& filelog = p4->FileLog(cl.number);
				cl.changedFiles = std::move(filelog.GetFileData());

				{
					std::unique_lock<std::mutex> lock((*(cl.canDownloadMutex)));
					*cl.canDownload = true;
				}
				cl.canDownloadCV->notify_all();
			});
	}
}

void ChangeList::StartDownload(const std::string& depotPath, const int& printBatch, const bool includeBinaries)
{
	ChangeList& cl = *this;

	ThreadPool::GetSingleton()->AddJob([&cl, &depotPath, printBatch, includeBinaries](P4API* p4)
	    {
		    // Wait for describe to finish, if it is still running
		    {
			    std::unique_lock<std::mutex> lock(*(cl.canDownloadMutex));
			    cl.canDownloadCV->wait(lock, [&cl]()
			        { return *(cl.canDownload) == true; });
		    }

		    *cl.filesDownloaded = 0;

		    if (cl.changedFiles.empty())
		    {
			    return;
		    }

		    std::shared_ptr<std::vector<std::string>> printBatchFiles = std::make_shared<std::vector<std::string>>();
		    std::shared_ptr<std::vector<FileData*>> printBatchFileData = std::make_shared<std::vector<FileData*>>();

		    for (int i = 0; i < cl.changedFiles.size(); i++)
		    {
			    FileData& fileData = cl.changedFiles[i];
			    if (p4->IsFileUnderDepotPath(fileData.depotFile, depotPath)
			        && p4->IsFileUnderClientSpec(fileData.depotFile)
			        && (includeBinaries || !p4->IsBinary(fileData.type))
			        && !STDHelpers::Contains(fileData.depotFile, "/.git/") // To avoid adding .git/ files in the Perforce history if any
			        && !STDHelpers::EndsWith(fileData.depotFile, "/.git")) // To avoid adding a .git submodule file in the Perforce history if any
			    {
				    fileData.shouldCommit = true;
				    printBatchFiles->push_back(fileData.depotFile + "#" + fileData.revision);
				    printBatchFileData->push_back(&fileData);
			    }
			    else
			    {
				    (*cl.filesDownloaded)++;
				    cl.commitCV->notify_all();
			    }

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

		    // Flush any remaining files that were smaller in number than the total batch size
		    if (!printBatchFiles->empty())
		    {
			    cl.Flush(printBatchFiles, printBatchFileData);
		    }
	    });
}

void ChangeList::Flush(std::shared_ptr<std::vector<std::string>> printBatchFiles, std::shared_ptr<std::vector<FileData*>> printBatchFileData)
{
	// Share ownership of this batch with the thread job
	ThreadPool::GetSingleton()->AddJob([this, printBatchFiles, printBatchFileData](P4API* p4)
	    {
		    const PrintResult& printData = p4->PrintFiles(*printBatchFiles);

		    for (int i = 0; i < printBatchFiles->size(); i++)
		    {
			    printBatchFileData->at(i)->contents = std::move(printData.GetPrintData().at(i).contents);
		    }

		    (*filesDownloaded) += printBatchFiles->size();

		    commitCV->notify_all();
	    });
}

void ChangeList::WaitForDownload()
{
	std::unique_lock<std::mutex> lock(*commitMutex);
	commitCV->wait(lock, [this]()
	    { return *(filesDownloaded) == (int)changedFiles.size(); });
}

void ChangeList::Clear()
{
	number.clear();
	user.clear();
	description.clear();
	changedFiles.clear();

	filesDownloaded.reset();
	canDownload.reset();
	canDownloadMutex.reset();
	canDownloadCV.reset();
	commitMutex.reset();
	commitCV.reset();
}
