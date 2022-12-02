/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "branch_set.h"
#include <stdexcept>
#include "branches/consuming_branch_model.h"


BranchSetBuilder::BranchSetBuilder(std::vector<std::string>& clientViewMapping)
    : m_streamsBranch(std::unique_ptr<AllStreamsBranchModel>(new AllStreamsBranchModel))
    , m_clientViewMapping(clientViewMapping)
    , m_closed(false)
{
}


void BranchSetBuilder::InsertStreamPath(const std::string streamPath)
{
    if (m_closed)
    {
        throw std::invalid_argument("Can't call the builder when it's been closed.");
    }
    m_streamsBranch->InsertStreamPath(streamPath);
}


void BranchSetBuilder::InsertBranchSpec(const std::string leftBranchName, const std::string rightBranchName, const std::vector<std::string> view)
{
    if (m_closed)
    {
        throw std::invalid_argument("Can't call the builder when it's been closed.");
    }
    m_branchSpecs.push_back(std::unique_ptr<BranchSpecBranchModel>(new BranchSpecBranchModel(leftBranchName, rightBranchName, view)));
}


std::vector<std::unique_ptr<BranchModel>> BranchSetBuilder::CompleteBranches(const std::string defaultBranchName)
{
    if (m_closed)
    {
        throw std::invalid_argument("Can't call the builder when it's been closed.");
    }
    if (!m_streamsBranch->IsEmpty())
    {
        m_branchSpecs.push_back(std::move(m_streamsBranch));
    }
    if (m_branchSpecs.empty())
    {
        // Require that at least one "branch" exists.
        m_branchSpecs.push_back(std::unique_ptr<BranchModel>(new ConsumingBranchModel(defaultBranchName)));
    }
    m_closed = true;
    return std::move(m_branchSpecs);
}



BranchSet::BranchSet(const std::string defaultBranchName, BranchSetBuilder& builder)
    : m_branches(builder.CompleteBranches(defaultBranchName))
{
    m_view.InsertTranslationMapping(builder.GetClientViewMapping());
}


std::vector<BranchedFiles> BranchSet::ParseAffectedFiles(std::vector<FileData>& cl) const
{
    std::vector<FileRevision> validRevs;
    for (int i = 0; i < cl.size(); i++)
    {
        FileData& fileData = cl.at(i);
        if (
            // depot file should always be present.
            // The left side of the client view is the depot side.
            m_view.IsInLeft(fileData.depotFile)
        )
        {
            // It's okay to add, but may not be a valid branch.
            FileRevision rev;
            rev.target = fileData.depotFile;
            rev.hasSource = false;
            rev.operation = (
                fileData.IsDeleted()
                    ? FileOperation::FileDeleted
                    : FileOperation::FileAltered);
            if (
                fileData.IsIntegrated()
                && !fileData.sourceDepotFile.empty()
                && m_view.IsInLeft(fileData.sourceDepotFile)
            )
            {
                // It's an allowed integration.
                rev.source = fileData.sourceDepotFile;
                rev.hasSource = true;
                rev.operation = FileOperation::FileMerged;
            }
            validRevs.push_back(rev);
        }
    }

    // We now have all the valid file operations.
    // Now let's find the destination branches.
    // Each one of these collections will become a Git commit.
    std::vector<BranchedFiles> ret;
    for (int i = 0; i < m_branches.size(); i++)
    {
        std::vector<BranchedFiles> branched = m_branches.at(i)->GetBranchedFiles(validRevs);
        ret.insert(ret.end(), branched.begin(), branched.end());
    }
    return ret;
}
