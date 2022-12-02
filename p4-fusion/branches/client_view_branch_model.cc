/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "client_view_branch_model.h"

ClientViewBranchModel::ClientViewBranchModel(const std::string& branchName, const std::vector<std::string>& mapping)
    : m_branchName(branchName)
{
    m_view.InsertTranslationMapping(mapping);
}

std::vector<BranchedFiles> ClientViewBranchModel::GetBranchedFiles(const std::vector<FileRevision> changedFiles) const
{
    BranchedFiles ret;
    ret.targetBranch = m_branchName;
    ret.hasSource = false;

    for (int i = 0; i < changedFiles.size(); i++)
    {
        const FileRevision& rev = changedFiles.at(i);
        std::string clientAbsPath = m_view.TranslateLeftToRight(rev.targetDepot);
        if (!clientAbsPath.empty())
        {
            // Set the relative location to the client 
            ret.files.push_back(rev);
        }
    }
    if (ret.files.empty())
    {
        // Free up a bit of memory.
        return {};
    }
    return { ret };
}
