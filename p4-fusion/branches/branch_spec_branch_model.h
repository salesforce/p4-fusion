/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>

#include "branch_model.h"
#include "file_map.h"


class BranchSpecBranchModel : public BranchModel
{
private:
    const std::string m_leftBranchName;
    const std::string m_rightBranchName;
    FileMap m_map;

public:
    BranchSpecBranchModel(const std::string leftBranchName, const std::string rightBranchName, const std::vector<std::string> view);
    ~BranchSpecBranchModel() override {};

    std::vector<BranchedFiles> GetBranchedFiles(const std::vector<FileRevision> changedFiles) const override;
};
