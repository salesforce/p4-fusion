/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "branch_spec_branch_model.h"

BranchSpecBranchModel::BranchSpecBranchModel(const std::string leftBranchName, const std::string rightBranchName, const std::vector<std::string> view)
    : m_leftBranchName(leftBranchName)
    , m_rightBranchName(rightBranchName)
{
    m_map.InsertTranslationMapping(view);
}

std::vector<BranchedFiles> BranchSpecBranchModel::GetBranchedFiles(const std::vector<FileRevision> changedFiles) const
{
    BranchedFiles leftToRight;
    leftToRight.sourceBranch = m_leftBranchName;
    leftToRight.hasSource = true;
    leftToRight.targetBranch = m_rightBranchName;

    BranchedFiles rightToLeft;
    rightToLeft.sourceBranch = m_rightBranchName;
    rightToLeft.hasSource = true;
    rightToLeft.targetBranch = m_leftBranchName;

    BranchedFiles justLeft;
    justLeft.hasSource = false;
    justLeft.targetBranch = m_leftBranchName;

    BranchedFiles justRight;
    justRight.hasSource = false;
    justRight.targetBranch = m_rightBranchName;

    for (int i = 0; i < changedFiles.size(); i++)
    {
        const FileRevision& rev = changedFiles.at(i);
        std::string targetFromLeft = m_map.TranslateLeftToRight(rev.target);
        std::string targetFromRight = m_map.TranslateRightToLeft(rev.target);

        // This changed file can only go in one place.  Do this checking carefully.
        if (!targetFromRight.empty())
        {
            // The affected file is on the right side.
            // If it's a merge, then the target is on the right side.  The source, if valid, will be on the left.
            if (rev.hasSource && rev.source == targetFromRight)
            {
                // It's a branch.
                leftToRight.files.push_back(rev);
            }
            else
            {
                // It's not a branch.  The files are just added to the right side.
                justRight.files.push_back(rev);
            }
        }
        else if (!targetFromLeft.empty())
        {
            // The affected file is on the left side.
            // If it's a merge, then the target is on the left side.  The source, if valid, will be on the right.
            if (rev.hasSource && rev.source == targetFromLeft)
            {
                // It's a branch.
                rightToLeft.files.push_back(rev);
            }
            else
            {
                // It's not a branch.  The files are just added to the left side.
                justLeft.files.push_back(rev);
            }
        }
        // Else it doesn't matter what the source is; the target isn't in view.
    }
    std::vector<BranchedFiles> ret;
    if (!leftToRight.files.empty())
    {
        ret.push_back(leftToRight);
    }
    if (!rightToLeft.files.empty())
    {
        ret.push_back(rightToLeft);
    }
    if (!justLeft.files.empty())
    {
        ret.push_back(justLeft);
    }
    if (!justRight.files.empty())
    {
        ret.push_back(justRight);
    }
    return ret;
}
