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

#include "common.h"
#include "git2/oid.h"

struct git_repository;

class GitAPI
{
	git_repository* m_Repo = nullptr;
	git_index* m_Index = nullptr;
	git_oid m_FirstCommitOid;

	std::string m_CurrentBranch = "";

public:
	GitAPI(bool fsyncEnable);
	~GitAPI();

	bool InitializeRepository(const std::string& srcPath);
	void OpenRepository(const std::string& repoPath);

	bool IsHEADExists();
	bool IsRepositoryClonedFrom(const std::string& depotPath);
	std::string DetectLatestCL();

	git_oid CreateBlob(const std::vector<char>& data);

	void CreateIndex();
	void SetActiveBranch(const std::string& branchName);
	void AddFileToIndex(const std::string& relativePath, const std::vector<char>& contents, const bool plusx);
	void RemoveFileFromIndex(const std::string& relativePath);

	std::string Commit(
	    const std::string& depotPath,
	    const std::string& cl,
	    const std::string& user,
	    const std::string& email,
	    const int& timezone,
	    const std::string& desc,
	    const int64_t& timestamp,
		const std::string& mergeFromStream);
	void CloseIndex();
};
