/*
 * Copyright (c) 2022 Salesforce, Inc.
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
#include "../branch_set.h"

class P4API;
class GitAPI;

struct ChangeList
{
	int number;
	std::string user;
	std::string description;
	int64_t timestamp = 0;
	std::unique_ptr<ChangedFileGroups> changedFileGroups = ChangedFileGroups::Empty();

	ChangeList(const int& clNumber, std::string&& clDescription, std::string&& userID, const int64_t& clTimestamp);
	ChangeList() = delete;
	ChangeList(const ChangeList& other) = delete;
	ChangeList& operator=(const ChangeList&) = delete;
	ChangeList(ChangeList&&) = default;
	ChangeList& operator=(ChangeList&&) = default;

	void PrepareDownload(P4API& p4, const BranchSet& branchSet);
	void StartDownload(P4API& p4, GitAPI& git, const int& printBatch);
	void WaitForDownload();
	void Clear();

private:
	std::shared_ptr<std::mutex> commitMutex = std::make_shared<std::mutex>();
	std::shared_ptr<std::atomic<bool>> downloadJobsCompleted = std::make_shared<std::atomic<bool>>(false);
	std::shared_ptr<std::condition_variable> commitCV = std::make_shared<std::condition_variable>();
	std::shared_ptr<std::atomic<bool>> downloadPrepared = std::make_shared<std::atomic<bool>>(false);
	std::shared_ptr<std::mutex> downloadPreparedMutex = std::make_shared<std::mutex>();
	std::shared_ptr<std::condition_variable> downloadPreparedCV = std::make_shared<std::condition_variable>();
};
