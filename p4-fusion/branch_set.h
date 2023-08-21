/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <stdexcept>

#include "commands/file_map.h"
#include "commands/file_data.h"
#include "utils/std_helpers.h"

struct BranchedFileGroup
{
	// If a BranchedFiles collection hasSource == true,
	// then all files in this collection MUST be a merge
	// from the given source branch to the target branch.
	// These branch names will be the Git branch names.
	std::string sourceBranch;
	std::string targetBranch;
	bool hasSource;
	std::vector<FileData> files;

	// Get all the relative file names from each of the file data.
	std::vector<std::string> GetRelativeFileNames();
};

struct ChangedFileGroups
{
private:
	ChangedFileGroups();

public:
	std::vector<BranchedFileGroup> branchedFileGroups;
	int totalFileCount;

	// When all the file groups have finished being used,
	// only then can we safely clear out the data.
	void Clear();

	ChangedFileGroups(std::vector<BranchedFileGroup>& groups, int totalFileCount);

	static std::unique_ptr<ChangedFileGroups> Empty() { return std::unique_ptr<ChangedFileGroups>(new ChangedFileGroups); };
};

struct Branch
{
public:
	const std::string depotBranchPath;
	const std::string gitAlias;

	Branch(const std::string& branch, const std::string& alias);

	// splitBranchPath If the relativeDepotPath matches, returns {branch alias, branch file path}.
	//   Otherwise, returns {"", ""}
	std::array<std::string, 2> SplitBranchPath(const std::string& relativeDepotPath) const;
};

// A singular view on the branches and a base view (acts as a filter to trim down affected files).
// Maps a changed file state to a list of resulting branches and affected files.
struct BranchSet
{
private:
	// Technically, these should all be const.
	const bool m_includeBinaries;
	std::string m_basePath;
	const std::vector<Branch> m_branches;
	FileMap m_view;

	// stripBasePath remove the base path from the depot path, or "" if not in the base path.
	std::string stripBasePath(const std::string& depotPath) const;

	// splitBranchPath extract the branch name and path under the branch (no leading '/' on the path)
	//    relativeDepotPath - already stripped from running stripBasePath.
	std::array<std::string, 2> splitBranchPath(const std::string& relativeDepotPath) const;

public:
	BranchSet(std::vector<std::string>& clientViewMapping, const std::string& baseDepotPath, const std::vector<std::string>& branches, const bool includeBinaries);

	// HasMergeableBranch is there a branch model that requires integration history?
	bool HasMergeableBranch() const { return !m_branches.empty(); };

	int Count() const { return m_branches.size(); };

	// ParseAffectedFiles create collections of merges and commits.
	// Breaks up the files into those that are within the view, with each item in the
	// list is its own target Git branch.
	// This also has the side-effect of populating the relative path value in the file data.
	//   ... the FileData object is copied, but it's underlying shared data is shared.  So, this
	//       breaks the const.
	std::unique_ptr<ChangedFileGroups> ParseAffectedFiles(const std::vector<FileData>& cl) const;
};
