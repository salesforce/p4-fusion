/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "branch_set.h"
#include <map>

static const std::string EMPTY_STRING = "";
static const std::array<std::string, 2> INVALID_BRANCH_PATH { EMPTY_STRING, EMPTY_STRING };

std::vector<std::string> BranchedFileGroup::GetRelativeFileNames()
{
	std::vector<std::string> ret;
	for (auto& fileData : files)
	{
		ret.push_back(fileData.GetRelativePath());
	}
	return ret;
}

ChangedFileGroups::ChangedFileGroups()
    : totalFileCount(0)
{
}

ChangedFileGroups::ChangedFileGroups(std::vector<BranchedFileGroup>& groups, int totalFileCount)
    : totalFileCount(totalFileCount)
{
	branchedFileGroups = std::move(groups);
}

void ChangedFileGroups::Clear()
{
	for (auto& fileGroup : branchedFileGroups)
	{
		for (auto& file : fileGroup.files)
		{
			file.Clear();
		}
		fileGroup.files.clear();
		fileGroup.sourceBranch.clear();
		fileGroup.targetBranch.clear();
	}
	totalFileCount = 0;
}

BranchSet::BranchSet(std::vector<std::string>& clientViewMapping, const std::string& baseDepotPath, const std::vector<std::string>& branches, const bool includeBinaries)
    : m_branches(branches)
    , m_includeBinaries(includeBinaries)
{
	m_view.InsertTranslationMapping(clientViewMapping);
	if (STDHelpers::EndsWith(baseDepotPath, "/..."))
	{
		// Keep the final '/'.
		m_basePath = baseDepotPath.substr(0, baseDepotPath.size() - 3);
	}
	else if (baseDepotPath.back() != '/')
	{
		throw std::invalid_argument("Bad base depot path format: " + baseDepotPath);
	}
	else
	{
		m_basePath = baseDepotPath;
	}
}

std::array<std::string, 2> BranchSet::splitBranchPath(const std::string& relativeDepotPath) const
{
	// Check if the relative depot path starts with any of the branches.
	// This checks the branches in their stored order, which can mean that having a branch
	// order like "//a/b/c" and "//a/b" will only work if the sub-branches are listed first.
	// To do this properly, the stored branches should be scanned based on their length - longest
	// first, but that's extra processing and code for a use case that is rare and has a manual
	// work around (list branches in a specific order).
	for (auto& branch : m_branches)
	{
		if (
		    // The relative depot branch, to match this branch path, must start with the
		    // branch path + "/".  The "StartsWith" is put at the end of the 'and' checks,
		    // because it takes the longest.
		    relativeDepotPath.size() > branch.size()
		    && relativeDepotPath[branch.size()] == '/'
		    && STDHelpers::StartsWith(relativeDepotPath, branch))
		{
			return { branch, relativeDepotPath.substr(branch.size() + 1) };
		}
	}
	return { "", "" };
}

std::string BranchSet::stripBasePath(const std::string& depotPath) const
{
	if (STDHelpers::StartsWith(depotPath, m_basePath))
	{
		// strip off the leading '/', too.
		return depotPath.substr(m_basePath.size());
	}
	return EMPTY_STRING;
}

struct branchIntegrationMap
{
	std::vector<BranchedFileGroup> branchGroups;
	std::unordered_map<std::string, int> branchIndicies;
	int fileCount = 0;

	void addMerge(const std::string& sourceBranch, const std::string& targetBranch, const FileData& rev);
	void addTarget(const std::string& targetBranch, const FileData& rev);

	// note: not const, because it cleans out the branchGroups.
	std::unique_ptr<ChangedFileGroups> createChangedFileGroups() { return std::unique_ptr<ChangedFileGroups>(new ChangedFileGroups(branchGroups, fileCount)); };
};

void branchIntegrationMap::addTarget(const std::string& targetBranch, const FileData& fileData)
{
	addMerge(EMPTY_STRING, targetBranch, fileData);
}

void branchIntegrationMap::addMerge(const std::string& sourceBranch, const std::string& targetBranch, const FileData& fileData)
{
	// Need to store this in the integration map, using "src/tgt" as the
	// key.  Because stream names can't have a '/' in them, this creates a unique key.
	// source might be empty, and that's okay.
	const std::string mapKey = sourceBranch + "/" + targetBranch;
	const auto entry = branchIndicies.find(mapKey);
	if (entry == branchIndicies.end())
	{
		const int index = branchGroups.size();
		branchIndicies.insert(std::make_pair(mapKey, index));
		branchGroups.push_back(BranchedFileGroup());
		BranchedFileGroup& bfg = branchGroups[index];
		bfg.sourceBranch = sourceBranch;
		bfg.targetBranch = targetBranch;
		bfg.hasSource = !sourceBranch.empty();
		bfg.files.push_back(fileData);
	}
	else
	{
		branchGroups.at(entry->second).files.push_back(fileData);
	}
	fileCount++;
}

bool BranchSet::isValidBranch(const std::string& name) const
{
	for (auto& branch : m_branches)
	{
		if (name == branch)
		{
			return true;
		}
	}
	return false;
}

// Post condition: all returned FileData (e.g. filtered for git commit) have the relativePath set.
std::unique_ptr<ChangedFileGroups> BranchSet::ParseAffectedFiles(const std::vector<FileData>& cl) const
{
	branchIntegrationMap branchMap;
	for (auto& clFileData : cl)
	{
		FileData fileData(clFileData);

		// First, filter out files we don't want.
		const std::string& depotFile = fileData.GetDepotFile();
		if (
		    // depot file should always be present.
		    // The left side of the client view is the depot side.
		    !m_view.IsInLeft(depotFile)
		    || (!m_includeBinaries && fileData.IsBinary())
		    || STDHelpers::Contains(depotFile, "/.git/") // To avoid adding .git/ files in the Perforce history if any
		    || STDHelpers::EndsWith(depotFile, "/.git") // To avoid adding a .git submodule file in the Perforce history if any
		)
		{
			continue;
		}
		std::string relativeDepotPath = stripBasePath(depotFile);
		if (relativeDepotPath.empty())
		{
			// Not under the depot path.  Shouldn't happen due to the way we
			// scan for files, but...
			continue;
		}

		// If we have branches, then possibly sort the file into a branch group.
		if (HasMergeableBranch())
		{
			// [0] == branch name, [1] == relative path in the branch.
			std::array<std::string, 2> branchPath = splitBranchPath(relativeDepotPath);
			if (
			    branchPath[0].empty()
			    || branchPath[1].empty()
			    || !isValidBranch(branchPath[0]))
			{
				// not a valid branch file.  skip it.
				continue;
			}

			// It's a valid destination to a branch.
			// Make sure the relative path is set.
			fileData.SetRelativePath(branchPath[1]);

			bool needsHandling = true;
			if (fileData.IsIntegrated())
			{
				// Only add the integration if the source is from a branch we care about.
				// [0] == branch name, [1] == relative path in the branch.
				std::array<std::string, 2> fromBranchPath = splitBranchPath(stripBasePath(fileData.GetFromDepotFile()));
				if (
				    !fromBranchPath[0].empty()
				    && !fromBranchPath[1].empty()
				    && isValidBranch(fromBranchPath[0])

				    // Can't have source and target be pointing to the same branch; that's not
				    // a branch operation in the Git sense.
				    && fromBranchPath[0] != branchPath[0])
				{
					// This is a valid integrate from a known source to a known target branch.
					branchMap.addMerge(fromBranchPath[0], branchPath[0], fileData);
					needsHandling = false;
				}
			}
			if (needsHandling)
			{
				// Either not a valid integrate, or a normal operation.
				branchMap.addTarget(branchPath[0], fileData);
			}
		}
		else
		{
			// It's a non-branching setup.
			// Make sure the relative path is set.
			fileData.SetRelativePath(relativeDepotPath);
			branchMap.addTarget(EMPTY_STRING, fileData);
		}
	}
	return branchMap.createChangedFileGroups();
}
