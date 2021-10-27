/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <memory>
#include <condition_variable>
#include <atomic>
#include <mutex>

#include "common.h"
#include "file_data.h"

struct ChangeList
{
	std::string number;
	std::string user;
	std::string description;
	long long timestamp;
	std::vector<FileData> changedFiles;
	std::shared_ptr<std::atomic<int>> filesDownloaded;
	std::shared_ptr<std::atomic<bool>> canDownload;
	std::shared_ptr<std::mutex> canDownloadMutex;
	std::shared_ptr<std::condition_variable> canDownloadCV;
	std::shared_ptr<std::mutex> commitMutex;
	std::shared_ptr<std::condition_variable> commitCV;

	void PrepareDownload();
	void StartDownload(const std::string& depotPath, const int& printBatch, const bool includeBinaries);
	void WaitForDownload();
	void Clear();
};
