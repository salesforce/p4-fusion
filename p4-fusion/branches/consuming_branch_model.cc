/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "consuming_branch_model.h"

ConsumingBranchModel::ConsumingBranchModel(const std::string& branchName)
    : m_branchName(branchName)
{
}

std::vector<BranchedFiles> ConsumingBranchModel::GetBranchedFiles(const std::vector<FileRevision> changedFiles) const
{
    BranchedFiles ret;
    ret.targetBranch = m_branchName;
    ret.hasSource = false;

    for (int i = 0; i < changedFiles.size(); i++)
    {
        ret.files.push_back(changedFiles.at(i));
    }
    if (ret.files.empty())
    {
        // Free up a bit of memory.
        return {};
    }
    return { ret };
}
