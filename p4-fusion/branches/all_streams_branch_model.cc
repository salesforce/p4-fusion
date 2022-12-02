/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "all_streams_branch_model.h"


void AllStreamsBranchModel::InsertStreamPath(const std::string streamPath)
{
    // slow and steady.
    // This is only run at initialization time, so doesn't need to be performant.
    for (int i = 0; i < m_streamDepots.size(); i++)
    {
        if (m_streamDepots.at(i)->IsStreamPathInDepot(streamPath))
        {
            if (m_streamDepots.at(i)->InsertStream(streamPath))
            {
                // Inserted the stream path.
                return;
            }
        }
    }
    // The depot is not in our existing collection.
    m_streamDepots.push_back(std::unique_ptr<StreamBranchModel>(new StreamBranchModel(streamPath, true)));
}


std::vector<BranchedFiles> AllStreamsBranchModel::GetBranchedFiles(const std::vector<FileRevision> changedFiles) const
{
    std::vector<BranchedFiles> ret;
    for (int i = 0; i < m_streamDepots.size(); i++)
    {
        std::vector<BranchedFiles> branched = m_streamDepots.at(i)->GetBranchedFiles(changedFiles);
        ret.insert(ret.end(), branched.begin(), branched.end());
    }
    return ret;
}
