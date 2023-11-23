/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>

#include "common.h"
#include "commands/file_data.h"
#include "commands/change_list.h"
#include "git2/oid.h"

struct git_repository;

class BlobWriter
{
private:
	git_repository* repo;
	git_writestream* writer;
	enum class State
	{
		Uninitialized,
		ReadyToWrite,
		Closed,
	} state;

public:
	BlobWriter() = delete;
	BlobWriter(git_repository* repo);

	// Write creates a new ODB entry on the first call and continuous calls keep
	// writing more data to it.
	void Write(const char* contents, int length);
	// Close MUST be called at the end of the writing process to finalize the ODB entry
	// and to move it into it's proper place on disk.
	std::string Close();
};

class GitAPI
{
	git_repository* m_Repo = nullptr;
	git_oid m_FirstCommitOid;
	std::string repoPath;
	int timezoneMinutes;
	std::unordered_map<std::string, git_index*> lastBranchTree;

public:
	GitAPI(const std::string& repoPath, int timezoneMinutes);
	GitAPI() = delete;
	~GitAPI();

	// WriteBlob returns a new BlobWriter instance that allows to write a single
	// blob to the repository's ODB.
	BlobWriter WriteBlob() const;

	// Init initializes the underlying libgit2 library.
	void Init(const bool fsyncEnable);
	void InitializeRepository(bool noCreateBaseCommit);
	void OpenRepository();
	bool IsHEADExists() const;
	bool IsRepositoryClonedFrom(const std::string& depotPath) const;
	/* Checks if a previous commit was made and extracts the corresponding changelist number. */
	const std::string DetectLatestCL() const;

	/* files are cleared as they are visited. Empty targetBranch means HEAD. Only use this method on the main thread! */
	std::string WriteChangelistBranch(
	    const std::string& depotPath,
	    const ChangeList& cl,
	    std::vector<FileData>& files,
	    const std::string& targetBranch,
	    const std::string& authorName,
	    const std::string& authorEmail,
	    const std::string& mergeFrom);
};
