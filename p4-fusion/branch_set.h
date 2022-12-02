/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>
#include <memory>

#include "branches/file_map.h"
#include "branches/branch_model.h"
#include "branches/all_streams_branch_model.h"
#include "branches/branch_spec_branch_model.h"
#include "commands/file_data.h"


// builder pattern for the branch set.
struct BranchSetBuilder
{
private:
    // TODO should this be a ref to the client view?  Could lead to memory issues.
    const std::vector<std::string> m_clientViewMapping;
    std::vector<std::unique_ptr<BranchModel>> m_branchSpecs;
    std::unique_ptr<AllStreamsBranchModel> m_streamsBranch;
    bool m_hasMergeableBranch;
    bool m_closed;

public:
    BranchSetBuilder(std::vector<std::string>& clientViewMapping);
    void InsertStreamPath(const std::string streamPath);
    void InsertBranchSpec(const std::string leftBranchName, const std::string rightBranchName, const std::vector<std::string> view);
    
    std::vector<std::string> GetClientViewMapping() const { return m_clientViewMapping; };
    std::vector<std::unique_ptr<BranchModel>> CompleteBranches(const std::string defaultBranchName);

    bool HasMergeableBranch() const { return m_hasMergeableBranch; };
};



// A singular view on the branches and a base view (acts as a filter to trim down affected files).
// Maps a changed file state to a list of resulting branches and affected files.
struct BranchSet
{
private:
    std::vector<std::unique_ptr<BranchModel>> m_branches;
    FileMap m_view;
    bool m_hasMergeableBranch;

public:
    BranchSet(const std::string defaultBranchName, BranchSetBuilder& builder);

    // HasMergeableBranch is there a branch model that requires integration history?
    bool HasMergeableBranch() const { return m_hasMergeableBranch; };

    // ParseAffectedFiles create collections of merges and commits.
    // Breaks up the files into those that are within the view, with each item in the
    // list is its own target Git branch.
    std::vector<BranchedFiles> ParseAffectedFiles(const std::vector<FileData>& cl) const;
};
