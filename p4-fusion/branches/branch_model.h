/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>


enum FileOperation {
    FileAltered,
    FileMerged,
    FileDeleted
};

struct FileRevision {
    // Deleted files have a target.
    // hasSource only has meaning for integrated files.
    std::string source;
    std::string target;
    bool hasSource;
    FileOperation operation;
};

struct BranchedFiles {
    // If a BranchedFiles collection hasSource == true,
    // then all files in this collection MUST be a merge
    // from the given source branch to the target branch.
    // These branch names will be the Git branch names.
    std::string sourceBranch;
    std::string targetBranch;
    bool hasSource;
    std::vector<FileRevision> files;
};


// Description of where files will be placed.
// All files must go under a git branch.  Some come from a different branch.
class BranchModel
{
public:
    virtual ~BranchModel() {};

    // GetBranchedFiles determine the file operations that were "branches" from one branch to another.
    // Each returned element group is one complete branch operation.
    virtual std::vector<BranchedFiles> GetBranchedFiles(const std::vector<FileRevision> changedFiles) const = 0;
};
