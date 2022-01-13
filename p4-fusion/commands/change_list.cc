/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "change_list.h"

#include "p4_api.h"
#include "describe_result.h"
#include "print_result.h"

void ChangeList::PrepareDownload(ThreadPool& thread_pool)
{
	ChangeList& cl = *this;

	thread_pool.AddJob([&cl](P4API* p4)
	    {
		    const DescribeResult& describe = p4->Describe(cl.number);
		    cl.changedFiles = std::move(describe.GetFileData());
		    {
			    std::unique_lock<std::mutex> lock((*(cl.canDownloadMutex)));
			    *cl.canDownload = true;
		    }
		    cl.canDownloadCV->notify_one();
	    });
}

void ChangeList::StartDownload(const std::string& depotPath, const int& printBatch, const bool includeBinaries, ThreadPool& thread_pool)
{
	ChangeList& cl = *this;
	auto download_job = [&cl, &depotPath, printBatch, includeBinaries, &thread_pool](P4API* p4)
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
			        && (fileData.depotFile.find("/.git/") == std::string::npos)) // To avoid adding .git files in the Perforce history if any
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

			    // Empty the batches
			    if (printBatchFiles->size() > printBatch || i == cl.changedFiles.size() - 1)
			    {
				    if (printBatchFiles->empty())
				    {
					    continue;
				    }

				    auto print_batch_job = [&cl, printBatchFiles, printBatchFileData](P4API* p4)
				        {
					        const PrintResult& printData = p4->PrintFiles(*printBatchFiles);

					        for (int i = 0; i < printBatchFiles->size(); i++)
					        {
						        printBatchFileData->at(i)->contents = std::move(printData.GetPrintData().at(i).contents);
					        }

					        (*cl.filesDownloaded) += printBatchFiles->size();

					        cl.commitCV->notify_all();
				        };
				    thread_pool.AddJob(std::move(print_batch_job));

				    printBatchFiles = std::make_shared<std::vector<std::string>>();
				    printBatchFileData = std::make_shared<std::vector<FileData*>>();
			    }
		    }
	    };
	thread_pool.AddJob(std::move(download_job));
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
	commitMutex.reset();
	commitCV.reset();
}
